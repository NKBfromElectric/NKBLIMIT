// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <vector>
#include <cstdint>
#include <cstddef>
namespace nkb {
// Preallocated monotonic deque. Includes current sample and previous window-1.
class PeakDetector {
public:
    void prepare(std::size_t window) { width = window; queue.resize(window + 1); reset(); }
    void reset() noexcept { head = tail = 0; clock = 0; }
    double push(double value) noexcept {
        while (head != tail && queue[head].time + width <= clock) head = next(head);
        while (head != tail && queue[previous(tail)].value <= value) tail = previous(tail);
        queue[tail] = {clock++, value}; tail = next(tail);
        return queue[head].value;
    }
private:
    struct Entry { std::uint64_t time = 0; double value = 0.0; };
    std::size_t next(std::size_t x) const noexcept { return (x + 1) % queue.size(); }
    std::size_t previous(std::size_t x) const noexcept { return (x + queue.size() - 1) % queue.size(); }
    std::vector<Entry> queue;
    std::size_t width = 1, head = 0, tail = 0;
    std::uint64_t clock = 0;
};
}
