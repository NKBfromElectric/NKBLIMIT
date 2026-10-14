// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <array>
#include <vector>
#include <cmath>
#include <numbers>
#include <algorithm>
// Independent offline 16x / 96-tap Kaiser-windowed sinc reconstruction.
// Does not reuse production meter or JUCE oversampling filters.
inline double referenceTruePeak(const std::vector<double>& samples) {
    constexpr int taps = 96, phases = 16;
    std::array<std::array<double,taps>,phases> filter {};
    auto i0 = [](double x) {
        double sum = 1.0, term = 1.0;
        for (int k = 1; k < 40; ++k) { term *= (x*x) / (4.0*k*k); sum += term; }
        return sum;
    };
    for (int phase = 0; phase < phases; ++phase) {
        double total = 0.0;
        for (int k = 0; k < taps; ++k) {
            const double d = k - 47.0 - double(phase)/phases;
            const double sinc = std::abs(d) < 1.0e-10 ? 1.0 : std::sin(std::numbers::pi*d)/(std::numbers::pi*d);
            const double r = 2.0*k/(taps-1)-1.0;
            filter[phase][k] = sinc*i0(9.0*std::sqrt(std::max(0.0,1.0-r*r)))/i0(9.0);
            total += filter[phase][k];
        }
        for (auto& coefficient : filter[phase]) coefficient /= total;
    }
    double maximum = 0.0;
    for (std::size_t n = 0; n < samples.size()+taps; ++n) {
        for (int phase = 0; phase < phases; ++phase) {
            double x = 0.0;
            for (int k = 0; k < taps; ++k)
                if (n >= std::size_t(k) && n-std::size_t(k) < samples.size())
                    x += samples[n-std::size_t(k)] * filter[phase][k];
            maximum = std::max(maximum,std::abs(x));
        }
    }
    return maximum;
}
