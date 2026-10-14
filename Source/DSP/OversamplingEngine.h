// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <juce_dsp/juce_dsp.h>
#include "LimiterEngine.h"
#include "TruePeakGuard.h"
#include <memory>
namespace nkb {
struct Parameters { double inputDb = 0.0, ceilingDb = -0.5; bool eight = false; };
struct Measurement { double input = 0.0, output = 0.0, truePeak = 0.0, reductionDb = 0.0; };
template <typename T> class OversamplingEngine {
    struct Path {
        std::unique_ptr<juce::dsp::Oversampling<T>> os;
        LimiterEngine<T> limiter;
        LookaheadBuffer<T> alignment;
        juce::AudioBuffer<T> buffer;
        std::vector<double> gains;
        int latency = 0;
    };
public:
    void prepare(double rate, int maximumBlock, Parameters initial = {}) {
        capacity = std::max(1, maximumBlock); sampleRate = rate;
        const int ahead = int(std::ceil(rate * 0.0025));
        int maximumLatency = 0;
        for (int p = 0; p < 2; ++p) {
            auto& path = paths[p];
            path.os = std::make_unique<juce::dsp::Oversampling<T>>(2, std::size_t(p + 2),
                juce::dsp::Oversampling<T>::filterHalfBandFIREquiripple, true, true);
            path.os->initProcessing(std::size_t(capacity));
            path.os->reset();
            path.buffer.setSize(2, capacity);
            path.gains.resize(std::size_t(capacity),1.0);
            const int factor = p == 0 ? 4 : 8;
            path.limiter.prepare(rate * factor, std::size_t(ahead * factor));
            path.latency = int(std::lround(path.os->getLatencyInSamples())) + ahead;
            maximumLatency = std::max(maximumLatency, path.latency);
        }
        for (auto& path : paths) path.alignment.prepare(std::size_t(maximumLatency - path.latency));
        latencySamples = maximumLatency + TruePeakGuard<T>::latency;
        input.prepare(rate, 20.0, initial.inputDb);
        ceiling.prepare(rate, 20.0, initial.ceilingDb);
        mix.prepare(rate, 20.0, initial.eight ? 1.0 : 0.0);
        ceilingValues.resize(std::size_t(capacity));
        mixValues.resize(std::size_t(capacity));
        guard.prepare(rate); meter.reset(); measurements = {};
    }
    void process(T* left, T* right, int count, Parameters params) noexcept {
        measurements = {};
        params.inputDb = std::clamp(sanitise(params.inputDb), -12.0, 24.0);
        params.ceilingDb = std::clamp(sanitise(params.ceilingDb), -3.0, 0.0);
        for (int offset = 0; offset < count; offset += capacity) {
            const int n = std::min(capacity, count - offset);
            for (int i = 0; i < n; ++i) {
                const double l = sanitise(left[offset+i]), r = sanitise(right[offset+i]);
                measurements.input = std::max({measurements.input, std::abs(l), std::abs(r)});
                const double drive = dbToGain(input.next(params.inputDb));
                ceilingValues[std::size_t(i)] = dbToGain(ceiling.next(params.ceilingDb));
                mixValues[std::size_t(i)] = mix.next(params.eight ? 1.0 : 0.0);
                for (auto& path : paths) {
                    path.buffer.setSample(0,i,T(l * drive)); path.buffer.setSample(1,i,T(r * drive));
                }
            }
            for (int p = 0; p < 2; ++p) {
                auto& path = paths[p];
                std::fill_n(path.gains.begin(),n,1.0);
                auto base = juce::dsp::AudioBlock<T>(path.buffer).getSubBlock(0,std::size_t(n));
                auto up = path.os->processSamplesUp(base);
                const auto factor = path.os->getOversamplingFactor();
                for (std::size_t i = 0; i < up.getNumSamples(); ++i) {
                    auto y = path.limiter.process({up.getChannelPointer(0)[i],up.getChannelPointer(1)[i]}, ceilingValues[i/factor]);
                    up.getChannelPointer(0)[i] = y[0]; up.getChannelPointer(1)[i] = y[1];
                    path.gains[i/factor] = std::min(path.gains[i/factor],path.limiter.lastGain);
                }
                path.os->processSamplesDown(base);
                for (int i = 0; i < n; ++i) {
                    auto y = path.alignment.push({base.getSample(0,std::size_t(i)),base.getSample(1,std::size_t(i))});
                    base.setSample(0,std::size_t(i),y[0]); base.setSample(1,std::size_t(i),y[1]);
                }
            }
            for (int i = 0; i < n; ++i) {
                const double a = mixValues[std::size_t(i)];
                std::array<T,2> x {};
                for (int c = 0; c < 2; ++c)
                    x[c] = T((1.0-a)*paths[0].buffer.getSample(c,i) + a*paths[1].buffer.getSample(c,i));
                auto y = guard.process(x,ceilingValues[std::size_t(i)]);
                left[offset+i] = y[0]; right[offset+i] = y[1];
                measurements.output = std::max({measurements.output,std::abs(double(y[0])),std::abs(double(y[1]))});
                measurements.truePeak = std::max(measurements.truePeak,meter.push(y[0],y[1]));
                const double selectedGain = (1.0-a)*paths[0].gains[std::size_t(i)] + a*paths[1].gains[std::size_t(i)];
                // Gain-control indication; FIR filtering makes this an estimate
                // of audible sample reduction, rather than a waveform ratio.
                measurements.reductionDb = std::max(measurements.reductionDb,
                    -gainToDb(guard.gain * selectedGain));
            }
        }
    }
    void reset(Parameters initial) noexcept {
        for (auto& path : paths) {
            path.os->reset(); path.limiter.reset(); path.alignment.reset(); path.buffer.clear();
        }
        input.prepare(sampleRate,20.0,initial.inputDb);
        ceiling.prepare(sampleRate,20.0,initial.ceilingDb);
        mix.prepare(sampleRate,20.0,initial.eight ? 1.0 : 0.0);
        guard.reset(); meter.reset(); measurements = {};
    }
    int latency() const noexcept { return latencySamples; }
    Measurement measurements;
private:
    Path paths[2];
    Smoother input, ceiling, mix;
    TruePeakGuard<T> guard;
    TruePeakMeter meter;
    std::vector<double> ceilingValues, mixValues;
    int capacity = 1, latencySamples = 0;
    double sampleRate = 48000.0;
};
}
