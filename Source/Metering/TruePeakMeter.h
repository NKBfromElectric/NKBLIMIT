// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "../DSP/Math.h"
#include <array>
namespace nkb {
// Windowed-sinc estimator following BS.1770 Annex 2 oversampling principle.
// This is NOT a certified BS.1770 meter: BS.2217 conformance remains pending.
class TruePeakMeter {
public:
    static constexpr int taps = 32, phases = 8;
    TruePeakMeter() {
        for (int p = 0; p < phases; ++p) {
            double sum = 0.0;
            for (int k = 0; k < taps; ++k) {
                const double t = k - 15.0 - double(p) / phases;
                const double sinc = std::abs(t) < 1.0e-12 ? 1.0 : std::sin(std::numbers::pi * t) / (std::numbers::pi * t);
                const double window = 0.42 - 0.5 * std::cos(2.0 * std::numbers::pi * k / (taps-1))
                                   + 0.08 * std::cos(4.0 * std::numbers::pi * k / (taps-1));
                coefficients[p][k] = sinc * window; sum += coefficients[p][k];
            }
            for (auto& h : coefficients[p]) h /= sum;
        }
    }
    void reset() noexcept { history = {}; position = 0; }
    double push(double l, double r, bool absoluteBound = false) noexcept {
        history[0][position] = l; history[1][position] = r;
        double peak = std::max(std::abs(l), std::abs(r));
        for (int c = 0; c < 2; ++c) for (int p = 0; p < phases; ++p) {
            double value = 0.0;
            for (int k = 0; k < taps; ++k) {
                const double term = coefficients[p][k] * history[c][(position + taps - k) % taps];
                value += absoluteBound ? std::abs(term) : term;
            }
            peak = std::max(peak, std::abs(value));
        }
        position = (position + 1) % taps;
        return peak;
    }
private:
    std::array<std::array<double,taps>,phases> coefficients {};
    std::array<std::array<double,taps>,2> history {};
    int position = 0;
};
}
