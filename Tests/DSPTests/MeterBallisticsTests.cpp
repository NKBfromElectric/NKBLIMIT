// SPDX-License-Identifier: AGPL-3.0-only
#include "UI/MeterBallistics.h"
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {
void check(bool condition, const char* description) {
    if (!condition) {
        std::cerr << "FAIL: " << description << '\n';
        std::exit(1);
    }
}
void near(float actual, float expected, const char* description) {
    check(std::abs(actual - expected) < 0.0002f, description);
}
}

int main() {
    nkb::MeterBallistics peak {-240.0f, 1.0, 12.0f};
    near(peak.get(), -240.0f, "initial display is silence");
    peak.push(-0.5f);
    peak.advance(0.2);
    near(peak.get(), -0.5f, "a brief peak survives the next numerical update");
    peak.push(-18.0f);
    peak.advance(0.8);
    near(peak.get(), -0.5f, "lower samples cannot erase the one-second peak hold");
    peak.advance(0.5);
    near(peak.get(), -6.5f, "peak falls 12 dB per second after the hold");
    peak.push(-2.0f);
    peak.advance(1.0);
    near(peak.get(), -2.0f, "a new high peak restarts the hold");
    peak.advance(1000.0);
    near(peak.get(), -240.0f, "decay stops at the display floor after a long pause");
    peak.push(std::numeric_limits<float>::quiet_NaN());
    peak.push(std::numeric_limits<float>::infinity());
    peak.advance(std::numeric_limits<double>::quiet_NaN());
    near(peak.get(), -240.0f, "non-finite measurements cannot corrupt the display");
    peak.push(-1.0f);
    peak.reset();
    near(peak.get(), -240.0f, "transport/processor reset clears stale display peaks");

    nkb::MeterBallistics gr {0.0f, 1.0, 6.0f};
    gr.push(8.0f);
    gr.advance(1.5);
    near(gr.get(), 5.0f, "gain reduction releases 6 dB per second after its hold");
    gr.advance(10.0);
    near(gr.get(), 0.0f, "gain reduction cannot decay below zero");

    nkb::MeterBallistics oneTick {-240.0f, 1.0, 12.0f};
    nkb::MeterBallistics manyTicks {-240.0f, 1.0, 12.0f};
    oneTick.push(-3.0f);
    manyTicks.push(-3.0f);
    oneTick.advance(2.0);
    for (int i = 0; i < 60; ++i) manyTicks.advance(1.0 / 30.0);
    near(manyTicks.get(), oneTick.get(), "display decay is independent of timer partitioning");
    std::cout << "Meter ballistics tests passed\n";
}
