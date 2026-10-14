// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Math.h"
namespace nkb {
class AdaptiveRelease {
public:
    void prepare(double rate) noexcept {
        sampleRate = rate;
        energy = heldPeak = transientWeight = 0.0;
        energyCoefficient = std::exp(-1.0 / (rate * 0.050));
        peakCoefficient = std::exp(-1.0 / (rate * 0.100));
    }

    // The detector describes the signal, independently of the input gain and
    // the amount of limiting. A deeper input drive must not, by itself, make
    // the release slower. A held peak gives a useful crest estimate between
    // drum hits; dividing RMS by the instantaneous sample did not do that.
    // A 50 ms energy estimate keeps a short kick from repeatedly changing the
    // crest-dependent attack blend and release. This reduces rapid modulation
    // of a linked sustained bed; dense, deeply limited material can still take
    // longer to recover between hits. Keep the release time bounds unchanged.
    double coefficient(double peak) noexcept {
        energy = energyCoefficient * energy + (1.0 - energyCoefficient) * peak * peak;
        heldPeak = std::max(peak, peakCoefficient * heldPeak);
        if (energy < 1.0e-30) energy = 0.0;
        if (heldPeak < 1.0e-15) heldPeak = 0.0;
        const double crest = heldPeak / std::max(std::sqrt(energy), 1.0e-15);
        transientWeight = std::clamp((crest - 1.65) / (2.80 - 1.65), 0.0, 1.0);

        // Tonal/low-crest material needs a long release to avoid following
        // individual bass cycles. High-crest peaks can recover much sooner;
        // GainEnvelope also carries a separate sustained reduction below.
        const double seconds = 0.200 + (0.006 - 0.200) * transientWeight;
        return std::exp(-1.0 / (sampleRate * seconds));
    }

    double transientWeight = 0.0;
private:
    double sampleRate = 48000.0;
    double energy = 0.0, heldPeak = 0.0;
    double energyCoefficient = 0.0, peakCoefficient = 0.0;
};
}
