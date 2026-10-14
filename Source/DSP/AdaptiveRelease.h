// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Math.h"
namespace nkb {
class AdaptiveRelease {
public:
    void prepare(double rate) noexcept {
        sampleRate = rate;
        energy.prepare(rate, 30.0, 0.0);
        duration.prepare(rate, 20.0, 0.100);
    }
    double coefficient(double peak, double reductionDb) noexcept {
        const double rms = std::sqrt(energy.next(peak * peak));
        const double sustained = std::clamp(rms / std::max(peak, 1.0e-9), 0.0, 1.0);
        const double seconds = duration.next(0.045 + 0.160 * sustained
                           + 0.006 * std::min(reductionDb, 18.0));
        return std::exp(-1.0 / (sampleRate * seconds));
    }
private:
    double sampleRate = 48000.0;
    Smoother energy, duration;
};
}
