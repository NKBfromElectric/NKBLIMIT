// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Math.h"
#include "LookaheadBuffer.h"
#include "PeakDetector.h"
#include <array>
#if defined(_M_X64) || defined(__SSE2__)
#include <emmintrin.h>
#endif
namespace nkb {
namespace detail {
// A signal-aware reconstruction predictor. This is validated against independent
// offline reconstruction, rather than an all-input mathematical TP bound.
template <typename T, int phases, bool vectorMath = true> class ReconstructionGuardStage {
public:
    static constexpr int taps = 128, attackSamples = 64;
    static constexpr int latency = taps - 1 + attackSamples;
    explicit ReconstructionGuardStage(double reserveDb) : reserveGain(dbToGain(-reserveDb)) {
        const double windowScale = besselI0(10.0);
        for (int p = 0; p < phases; ++p) {
            double sum = 0.0;
            for (int k = 0; k < taps; ++k) {
                const double t = k - 63.0 - double(p) / phases;
                const double sinc = std::abs(t) < 1.0e-12 ? 1.0 : std::sin(std::numbers::pi * t) / (std::numbers::pi * t);
                const double r = 2.0 * k / (taps - 1) - 1.0;
                coefficients[p][k] = sinc * besselI0(10.0 * std::sqrt(std::max(0.0, 1.0 - r * r))) / windowScale;
                sum += coefficients[p][k];
            }
            for (auto& h : coefficients[p]) h /= sum;
            // Contiguous forward history loads allow double-precision SIMD.
            std::reverse(coefficients[p].begin(), coefficients[p].end());
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
        history[0][position] = history[0][position + taps] = double(x[0]);
        history[1][position] = history[1][position + taps] = double(x[1]);
        double peak = std::max(std::abs(double(x[0])), std::abs(double(x[1])));
        for (int c = 0; c < 2; ++c) for (int p = 0; p < phases; ++p)
            peak = std::max(peak, std::abs(reconstruct(coefficients[p].data(), history[c].data() + position + 1)));
        position = (position + 1) % taps;
        const double predicted = peaks.push(peak);
        const double protectedCeiling = ceiling * reserveGain;
        inversePeaks[targetPosition] = inversePeaks[targetPosition + attackSamples + 1] = 1.0 / std::max(predicted, 1.0e-12);
        double limit = 1.0;
        for (int age = 0; age <= attackSamples; ++age) {
            const auto slot = targetPosition + attackSamples + 1 - std::size_t(age);
            const double target = std::min(1.0, inversePeaks[slot] * protectedCeiling);
            // Reach each measured constraint after a linear lookahead attack.
            limit = std::min(limit, target + (1.0 - target) * (attackSamples - age) / attackSamples);
        }
        targetPosition = (targetPosition + 1) % (attackSamples + 1);
        gain = std::min(limit, 1.0 + release * (gain - 1.0));
        auto y = delay.push(x);
        y[0] = T(y[0] * gain); y[1] = T(y[1] * gain); return y;
    }
    double gain = 1.0;
private:
    static double reconstruct(const double* h, const double* x) noexcept {
#if defined(_M_X64) || defined(__SSE2__)
        if constexpr (vectorMath) {
            // Four independent accumulators preserve the filter and every
            // phase, while avoiding a scalar reduction dependency per tap.
            __m128d a = _mm_setzero_pd(), b = a, c = a, d = a;
            for (int k = 0; k < taps; k += 8) {
                a = _mm_add_pd(a, _mm_mul_pd(_mm_loadu_pd(h + k), _mm_loadu_pd(x + k)));
                b = _mm_add_pd(b, _mm_mul_pd(_mm_loadu_pd(h + k + 2), _mm_loadu_pd(x + k + 2)));
                c = _mm_add_pd(c, _mm_mul_pd(_mm_loadu_pd(h + k + 4), _mm_loadu_pd(x + k + 4)));
                d = _mm_add_pd(d, _mm_mul_pd(_mm_loadu_pd(h + k + 6), _mm_loadu_pd(x + k + 6)));
            }
            const auto sum = _mm_add_pd(_mm_add_pd(a,b), _mm_add_pd(c,d));
            return _mm_cvtsd_f64(_mm_add_sd(sum, _mm_unpackhi_pd(sum,sum)));
        }
#endif
        double value = 0.0;
        for (int k = 0; k < taps; ++k) value += h[k] * x[k];
        return value;
    }
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
    const double reserveGain;
    double release = 0.0;
};
}
// The first stage limits the incoming reconstructed waveform. A second stage
// measures that actual gain-modulated result and corrects its remaining peaks.
// Small 0.02/0.06 dB reserves replace the previous blanket 0.5 dB attenuation.
template <typename T> class TruePeakGuard {
public:
    static constexpr int latency = detail::ReconstructionGuardStage<T,16>::latency
        + detail::ReconstructionGuardStage<T,8>::latency;
    void prepare(double rate) { first.prepare(rate); second.prepare(rate); reset(); }
    void reset() noexcept {
        first.reset(); second.reset();
        firstGains.fill(1.0); gainPosition = 0; gain = 1.0;
    }
    std::array<T,2> process(std::array<T,2> x, double ceiling) noexcept {
        const auto y = second.process(first.process(x,ceiling),ceiling);
        firstGains[gainPosition] = first.gain;
        gainPosition = (gainPosition + 1) % firstGains.size();
        // Align the first reduction to the samples leaving the second delay.
        gain = firstGains[gainPosition] * second.gain;
        return y;
    }
    double gain = 1.0;
private:
    detail::ReconstructionGuardStage<T,16> first {0.02};
    detail::ReconstructionGuardStage<T,8> second {0.06};
    std::array<double,detail::ReconstructionGuardStage<T,8>::latency + 1> firstGains {};
    std::size_t gainPosition = 0;
};
}
