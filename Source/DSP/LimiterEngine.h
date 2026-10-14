// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "LookaheadBuffer.h"
#include "PeakDetector.h"
#include "GainEnvelope.h"
namespace nkb {
// Phase 1: Transparent. Stereo linked, predictive gain control, no waveshaper.
template <typename T> class LimiterEngine {
public:
    void prepare(double rate, std::size_t lookahead) {
        sampleRate = rate;
        delay.prepare(lookahead); peaks.prepare(lookahead + 1); envelope.prepare(rate);
    }
    void reset() noexcept { delay.reset(); peaks.reset(); envelope.prepare(sampleRate); lastGain = 1.0; }
    std::array<T,2> process(std::array<T,2> x, double ceiling) noexcept {
        const double peak = std::max(std::abs(double(x[0])), std::abs(double(x[1])));
        const double futurePeak = peaks.push(peak);
        const double requested = std::min(1.0, ceiling / std::max(futurePeak, 1.0e-12));
        auto y = delay.push(x);
        double gain = envelope.process(requested, peak);
        const double delayedPeak = std::max(std::abs(double(y[0])), std::abs(double(y[1])));
        // Exact constraint for exceptionally steep/large attacks. Normal attacks
        // have settled through 2.5 ms of prediction (25 attack time constants).
        gain = std::min(gain, ceiling / std::max(delayedPeak, 1.0e-12));
        lastGain = gain;
        y[0] = T(y[0] * gain); y[1] = T(y[1] * gain);
        return y;
    }
    double lastGain = 1.0;
private:
    LookaheadBuffer<T> delay;
    PeakDetector peaks;
    GainEnvelope envelope;
    double sampleRate = 48000.0;
};
}
