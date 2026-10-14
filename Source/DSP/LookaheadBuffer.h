// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <array>
#include <vector>
#include <cstddef>
#include <algorithm>
namespace nkb {
template <typename T> class LookaheadBuffer {
public:
    void prepare(std::size_t delay) {
        samples.assign(delay + 1, {}); position = 0; delaySamples = delay;
    }
    void reset() noexcept { std::fill(samples.begin(), samples.end(), std::array<T,2>{}); position = 0; }
    std::array<T,2> push(std::array<T,2> input) noexcept {
        samples[position] = input;
        const auto result = samples[(position + 1) % samples.size()];
        position = (position + 1) % samples.size();
        return result;
    }
    std::size_t latency() const noexcept { return delaySamples; }
private:
    std::vector<std::array<T,2>> samples;
    std::size_t position = 0, delaySamples = 0;
};
}
