// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <algorithm>
#include <cmath>

namespace nkb {
// Message-thread display ballistics. Audio measurements remain untouched.
class MeterBallistics {
public:
    MeterBallistics(float floor, double holdSeconds, float fallDbPerSecond) noexcept
        : floorValue(floor), holdTime(holdSeconds), fallRate(fallDbPerSecond), value(floor) {}
    void reset() noexcept { value = floorValue; holdRemaining = 0.0; }
    void push(float peak) noexcept {
        if (!std::isfinite(peak)) return;
        peak = std::max(floorValue, peak);
        if (peak >= value) { value = peak; holdRemaining = holdTime; }
    }
    void advance(double elapsedSeconds) noexcept {
        if (!std::isfinite(elapsedSeconds) || elapsedSeconds <= 0.0) return;
        const auto held = std::min(holdRemaining, elapsedSeconds);
        holdRemaining -= held;
        value = std::max(floorValue, value - fallRate * static_cast<float>(elapsedSeconds - held));
    }
    float get() const noexcept { return value; }
private:
    float floorValue;
    double holdTime;
    float fallRate;
    float value;
    double holdRemaining = 0.0;
};
} // namespace nkb
