// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "../Metering/TruePeakMeter.h"
#include "LookaheadBuffer.h"
#include "PeakDetector.h"
namespace nkb {
// Conservative reconstruction bound, operating AFTER downsampling/crossfade.
// Each output sample gets gain <= every bound whose FIR support contains it.
// Unlike a signed peak predictor this remains safe under gain modulation.
template <typename T> class TruePeakGuard {
public:
    static constexpr int latency = TruePeakMeter::taps - 1;
    void prepare(double rate) {
        meter.reset(); delay.prepare(latency); peaks.prepare(TruePeakMeter::taps);
        release = std::exp(-1.0 / (rate * 0.08)); gain = 1.0;
    }
    void reset() noexcept { meter.reset(); delay.reset(); peaks.reset(); gain = 1.0; }
    std::array<T,2> process(std::array<T,2> x, double ceiling) noexcept {
        const double bound = peaks.push(meter.push(x[0], x[1], true));
        // Extra 0.3 dB allowance for longer independent reconstruction filters.
        const double target = std::min(1.0, ceiling * dbToGain(-0.3) / std::max(bound, 1.0e-12));
        gain = target < gain ? target : target + release * (gain - target);
        auto y = delay.push(x);
        y[0] = T(y[0] * gain); y[1] = T(y[1] * gain); return y;
    }
    double gain = 1.0;
private:
    TruePeakMeter meter;
    LookaheadBuffer<T> delay;
    PeakDetector peaks;
    double release = 0.0;
};
}
