// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "AdaptiveRelease.h"
namespace nkb {
class GainEnvelope {
public:
    void prepare(double rate) noexcept {
        gain = 1.0; attack = std::exp(-1.0 / (rate * 0.0001)); release.prepare(rate);
    }
    double process(double requested, double peak) noexcept {
        const double r = release.coefficient(peak, -gainToDb(gain));
        const double coefficient = requested < gain ? attack : r;
        gain = requested + coefficient * (gain - requested);
        if (std::abs(gain - requested) < 1.0e-12) gain = requested;
        return gain;
    }
    double gain = 1.0;
private:
    double attack = 0.0;
    AdaptiveRelease release;
};
}
