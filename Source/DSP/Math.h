// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <algorithm>
#include <cmath>
#include <numbers>
namespace nkb {
inline double dbToGain(double db) noexcept { return std::pow(10.0, db / 20.0); }
inline double gainToDb(double x) noexcept { return x > 1.0e-12 ? 20.0 * std::log10(x) : -240.0; }
inline double sanitise(double x) noexcept {
    return std::isfinite(x) ? std::clamp(x, -1.0e6, 1.0e6) : 0.0;
}
class Smoother {
public:
    void prepare(double rate, double ms, double initial) noexcept {
        coefficient = std::exp(-1.0 / (rate * ms * 0.001)); value = initial;
    }
    double next(double target) noexcept {
        value = target + coefficient * (value - target);
        if (std::abs(value - target) < 1.0e-12) value = target;
        return value;
    }
    double value = 0.0;
private:
    double coefficient = 0.0;
};
}
