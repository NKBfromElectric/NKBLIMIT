// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "LookaheadBuffer.h"
#include "PeakDetector.h"
#include "GainEnvelope.h"
#include <cstdint>
#include <vector>
namespace nkb {
// Phase 1: Transparent. Stereo linked, predictive gain control, no waveshaper.
template <typename T> class LimiterEngine {
public:
    void prepare(double rate, std::size_t lookahead) {
        sampleRate = std::isfinite(rate) && rate > 0.0 ? rate : 48000.0;
        delay.prepare(lookahead); peaks.prepare(lookahead + 1);
        prediction.prepare(lookahead); envelope.prepare(sampleRate); lastGain = 1.0;
    }
    void reset() noexcept {
        delay.reset(); peaks.reset(); prediction.reset(); envelope.prepare(sampleRate); lastGain = 1.0;
    }
    std::array<T,2> process(std::array<T,2> x, double ceiling) noexcept {
        x[0] = T(sanitise(double(x[0]))); x[1] = T(sanitise(double(x[1])));
        ceiling = std::isfinite(ceiling) && ceiling > 0.0 ? ceiling : 1.0;
        const double peak = std::max(std::abs(double(x[0])), std::abs(double(x[1])));
        const double reductionDb = std::max(0.0, gainToDb(peak / ceiling));
        const double fullReductionDb = peaks.push(reductionDb);
        const double predictiveReductionDb = prediction.push(reductionDb);
        auto y = delay.push(x);
        double gain = envelope.process(fullReductionDb, predictiveReductionDb, peak);
        const double delayedPeak = std::max(std::abs(double(y[0])), std::abs(double(y[1])));
        // Both detector paths include the exact demand for this delayed sample.
        // Retain the sample bound as a guard against floating-point roundoff.
        gain = std::min(gain, ceiling / std::max(delayedPeak, 1.0e-12));
        lastGain = gain;
        y[0] = T(y[0] * gain); y[1] = T(y[1] * gain);
        return y;
    }
    double lastGain = 1.0;
private:
    // Preallocated monotonic deque of future dB constraints. Each sample's
    // required reduction rises by 16 dB over the lookahead interval, reaching
    // its exact demand when that sample leaves the delay. Unlike a plain window
    // maximum, this does not impose full peak reduction on all earlier audio.
    class PredictiveReduction {
    public:
        void prepare(std::size_t lookahead) {
            width = lookahead + 1;
            queue.resize(width + 1);
            slope = 16.0 / static_cast<double>(std::max(std::size_t(1), lookahead));
            reset();
        }
        void reset() noexcept { head = tail = 0; clock = 0; }
        double push(double reductionDb) noexcept {
            while (head != tail && clock - queue[head].time >= width) head = next(head);
            while (head != tail) {
                const auto& back = queue[previous(tail)];
                if (back.reductionDb + slope * static_cast<double>(clock - back.time) > reductionDb) break;
                tail = previous(tail);
            }
            queue[tail] = {reductionDb, clock}; tail = next(tail);
            const auto& earliest = queue[head];
            const auto untilOutput = width - 1 - static_cast<std::size_t>(clock - earliest.time);
            const double result = std::max(0.0, earliest.reductionDb - slope * static_cast<double>(untilOutput));
            ++clock;
            return result;
        }
    private:
        struct Entry { double reductionDb = 0.0; std::uint64_t time = 0; };
        std::size_t next(std::size_t i) const noexcept { return (i + 1) % queue.size(); }
        std::size_t previous(std::size_t i) const noexcept { return (i + queue.size() - 1) % queue.size(); }
        std::vector<Entry> queue;
        std::size_t width = 1, head = 0, tail = 0;
        std::uint64_t clock = 0;
        double slope = 0.0;
    };
    LookaheadBuffer<T> delay;
    PeakDetector peaks;
    PredictiveReduction prediction;
    GainEnvelope envelope;
    double sampleRate = 48000.0;
};
}
