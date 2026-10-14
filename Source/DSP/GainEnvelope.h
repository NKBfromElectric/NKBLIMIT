// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "AdaptiveRelease.h"
namespace nkb {
class GainEnvelope {
public:
    void prepare(double rate) noexcept {
        gain = 1.0;
        peakReductionDb = sustainedReductionDb = 0.0;
        sustainedAttack = std::exp(-1.0 / (rate * 0.015));
        sustainedRelease = std::exp(-1.0 / (rate * 0.090));
        release.prepare(rate);
    }

    // fullReductionDb is the conservative full lookahead-window demand;
    // predictiveReductionDb reaches each peak's exact demand at its deadline,
    // with a finite dB ramp beforehand. Their blend remains a valid peak bound.
    double process(double fullReductionDb, double predictiveReductionDb, double peak) noexcept {
        const double peakRelease = release.coefficient(peak);
        const double requestedDb = fullReductionDb
                                + release.transientWeight * (predictiveReductionDb - fullReductionDb);

        // The lookahead ramp supplies the attack. An additional attack filter
        // would lag that guaranteed bound and require clipping at the deadline.
        if (requestedDb >= peakReductionDb) peakReductionDb = requestedDb;
        else peakReductionDb = requestedDb + peakRelease * (peakReductionDb - requestedDb);

        // Keep a slower, quieter reduction for sustained limiting, while the
        // fast path handles isolated peaks. Work in dB so the recovery shape
        // does not change as an accidental consequence of the gain depth.
        const double sustainedTarget = 0.600 * requestedDb;
        const double coefficient = sustainedTarget > sustainedReductionDb ? sustainedAttack : sustainedRelease;
        sustainedReductionDb = sustainedTarget + coefficient * (sustainedReductionDb - sustainedTarget);
        if (peakReductionDb < 1.0e-10) peakReductionDb = 0.0;
        if (sustainedReductionDb < 1.0e-10) sustainedReductionDb = 0.0;
        gain = dbToGain(-std::max(peakReductionDb, sustainedReductionDb));
        return gain;
    }
    double gain = 1.0;
private:
    double peakReductionDb = 0.0, sustainedReductionDb = 0.0;
    double sustainedAttack = 0.0, sustainedRelease = 0.0;
    AdaptiveRelease release;
};
}
