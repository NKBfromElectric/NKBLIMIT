// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Math.h"
#include "LookaheadBuffer.h"
#include "PeakDetector.h"
#include <array>
namespace nkb {
// Signal-aware reconstruction protection after downsampling / path mixing.
// Lookahead allows a linear attack before a reconstructed peak; release and
// the reconstruction-support hold avoid abrupt sample-to-sample gain changes.
// This is a tested peak predictor, not an all-input mathematical bound.
template <typename T> class TruePeakGuard {
public:
    static constexpr int taps = 128, phases = 8, attackSamples = 64;
    static constexpr int latency = taps - 1 + attackSamples;
    TruePeakGuard() {
        for (int p = 0; p < phases; ++p) {
            double sum = 0.0;
            for (int k = 0; k < taps; ++k) {
                const double t = k - 63.0 - double(p) / phases;
                const double sinc = std::abs(t) < 1.0e-12 ? 1.0 : std::sin(std::numbers::pi * t) / (std::numbers::pi * t);
                const double r = 2.0 * k / (taps - 1) - 1.0;
                coefficients[p][k] = sinc * besselI0(10.0 * std::sqrt(std::max(0.0, 1.0 - r * r))) / besselI0(10.0);
                sum += coefficients[p][k];
            }
            for (auto& h : coefficients[p]) h /= sum;
        }
        reset();
    }
    void prepare(double rate) {
        delay.prepare(latency); peaks.prepare(taps);
        release = std::exp(-1.0 / (rate * 0.032)); reset();
    }
    void reset() noexcept {
        history = {}; position = targetPosition = 0;
        inversePeaks.fill(1.0e12);
        delay.reset(); peaks.reset(); gain = 1.0;
    }
    std::array<T,2> process(std::array<T,2> x, double ceiling) noexcept {
        history[0][position] = history[0][position + taps] = double(x[0]); history[1][position] = history[1][position + taps] = double(x[1]);
        double peak = std::max(std::abs(double(x[0])), std::abs(double(x[1])));
        for (int c = 0; c < 2; ++c) for (int p = 0; p < phases; ++p) {
            double value = 0.0;
            for (int k = 0; k < taps; ++k)
                value += coefficients[p][k] * history[c][position + taps - k];
            peak = std::max(peak, std::abs(value));
        }
        position = (position + 1) % taps;
        const double predicted = peaks.push(peak);
        // Reserved for gain modulation and reconstruction-filter differences.
        const double protectedCeiling = ceiling * dbToGain(-0.5);
        inversePeaks[targetPosition] = inversePeaks[targetPosition + attackSamples + 1] = 1.0 / std::max(predicted, 1.0e-12);
        double limit = 1.0;
        for (int age = 0; age <= attackSamples; ++age) {
            const auto slot = targetPosition + attackSamples + 1 - std::size_t(age);
            const double target = std::min(1.0, inversePeaks[slot] * protectedCeiling);
            // A constraint measured now is due after attackSamples. Its ramp
            // starts at unity and reaches the required gain at that deadline.
            limit = std::min(limit, target + (1.0 - target) * (attackSamples - age) / attackSamples);
        }
        targetPosition = (targetPosition + 1) % (attackSamples + 1);
        gain = std::min(limit, 1.0 + release * (gain - 1.0));
        auto y = delay.push(x);
        y[0] = T(y[0] * gain); y[1] = T(y[1] * gain); return y;
    }
    double gain = 1.0;
private:
    static double besselI0(double x) noexcept {
        double sum = 1.0, term = 1.0;
        for (int k = 1; k < 40; ++k) { term *= x * x / (4.0 * k * k); sum += term; }
        return sum;
    }
    std::array<std::array<double,taps>,phases> coefficients {};
    std::array<std::array<double,taps * 2>,2> history {};
    std::array<double,(attackSamples + 1) * 2> inversePeaks {};
    std::size_t position = 0, targetPosition = 0;
    LookaheadBuffer<T> delay;
    PeakDetector peaks;
    double release = 0.0;
};
}
