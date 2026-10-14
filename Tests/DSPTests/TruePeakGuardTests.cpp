// SPDX-License-Identifier: AGPL-3.0-only
#include "DSP/TruePeakGuard.h"
#include "ReferenceTruePeak.h"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <new>
#include <random>
#include <stdexcept>
#include <type_traits>
static std::atomic<std::size_t> allocations {0};
void* operator new(std::size_t n) { allocations.fetch_add(1); if (auto* p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc(); }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
static void require(bool passed, const char* message) { if (!passed) throw std::runtime_error(message); }
static double worst = -240.0;
template <typename T> static void stress() {
    std::mt19937 random(51638);
    constexpr int count = 384, length = 4096;
    constexpr double rates[] {44100.,48000.,88200.,96000.,192000.};
    constexpr double ceilingDb[] {-3.,-.5,0.};
    for (int test = 0; test < count; ++test) {
        const double rate = rates[test % 5], ceiling = nkb::dbToGain(ceilingDb[test % 3]);
        nkb::TruePeakGuard<T> guard; guard.prepare(rate);
        std::vector<double> left(length + 384), right(length + 384);
        const double frequency = .0001 + double(random() % 49940) / 100000.;
        const double phase = double(random() % 10000) / 10000. * 2. * std::numbers::pi;
        const double amplitude = .1 + double(random() % 20000) / 100.;
        const auto allocationCount = allocations.load();
        double ratioError = 0.0;
        for (int i = 0; i < length + 384; ++i) {
            double l = 0.0, r = 0.0;
            if (i < length) {
                switch (test % 8) {
                    case 0: l = amplitude * std::sin(2. * std::numbers::pi * frequency * i + phase); break;
                    case 1: l = (i % 71 < 21 ? amplitude : .1) * std::sin(2. * std::numbers::pi * frequency * i + phase); break;
                    case 2: l = i % 201 == 0 ? amplitude * (random() % 2 ? 1. : -1.) : 0.; break;
                    case 3: l = i < length / 2 ? amplitude : -amplitude; break;
                    case 4: l = double(int(random() % 20001) - 10000) / 10000. * amplitude; break;
                    case 5: l = amplitude * (std::sin(2. * std::numbers::pi * frequency * i + phase) + std::sin(2. * std::numbers::pi * .147 * i + .7)); break;
                    case 6: l = (i > 1700 && i < 2000 ? amplitude : .07) * std::sin(2. * std::numbers::pi * .499 * i + phase); break;
                    case 7: l = amplitude * std::sin(2. * std::numbers::pi * (frequency * i + .4 * i * i / length)); break;
                }
                r = test % 3 == 0 ? .37 * l : (test % 3 == 1 ? -.41 * l : amplitude * std::cos(.121 * i));
            }
            const auto output = guard.process({T(l),T(r)},ceiling);
            left[std::size_t(i)] = double(output[0]); right[std::size_t(i)] = double(output[1]);
            if (test % 3 < 2) ratioError = std::max(ratioError, std::abs(double(output[1]) - double(output[0]) * (test % 3 == 0 ? .37 : -.41)));
            require(std::isfinite(output[0]) && std::isfinite(output[1]),"guard produced non-finite output");
        }
        require(allocations.load() == allocationCount,"guard process allocated");
        require(ratioError < (std::is_same_v<T,float> ? 3.e-6 : 1.e-10),"guard changed stereo ratio");
        const double excess = nkb::gainToDb(std::max(referenceTruePeak(left), referenceTruePeak(right))) - ceilingDb[test % 3];
        worst = std::max(worst,excess);
        if (excess > .1) std::cerr << "guard TP excess case=" << test << " rate=" << rate << " excess=" << excess << '\n';
        require(excess <= .1,"guard exceeded independent reconstruction ceiling + 0.1 dB");
        const auto beforeReset = allocations.load(); guard.reset();
        require(allocations.load() == beforeReset,"guard reset allocated");
    }
}
template <typename T> static void behaviour() {
    nkb::TruePeakGuard<T> guard; guard.prepare(48000.);
    // A saturated steady tone must approach the requested ceiling: the old
    // absolute-contribution FIR bound attenuated an otherwise valid waveform.
    double peak = 0.0;
    for (int i = 0; i < 16384; ++i) {
        const auto y = guard.process({T(2. * std::sin(2. * std::numbers::pi * 997 * i / 48000.)),T{}},nkb::dbToGain(-.5));
        if (i >= 4096) peak = std::max(peak,std::abs(double(y[0])));
    }
    require(nkb::gainToDb(peak) > -1.02,"guard applies excessive steady-state headroom");
    guard.reset();
    // Changing the ceiling under the signal's peak must preserve unity gain.
    for (int i = 0; i < 8192; ++i) {
        const auto y = guard.process({T(.03 * std::sin(i * .13)),T{}},nkb::dbToGain(i < 2000 ? 0. : -3.));
        if (i >= nkb::TruePeakGuard<T>::latency) {
            const double expected = double(T(.03 * std::sin((i - nkb::TruePeakGuard<T>::latency) * .13)));
            require(std::abs(double(y[0]) - expected) < 1.e-8,"quiet signal attenuated during ceiling change");
        }
    }
    guard.reset();
    int observed = -1;
    for (int i = 0; i < 512; ++i) {
        const auto y = guard.process({T(i == 100 ? .001 : 0.),T{}},1.);
        if (y[0] != T{}) observed = i - 100;
    }
    require(observed == nkb::TruePeakGuard<T>::latency,"guard latency disagrees with output");
}
int main() {
    try {
        behaviour<float>(); behaviour<double>(); stress<float>(); stress<double>();
        std::cout << "PASS TruePeakGuard cases=768 worst_reference_excess_dB=" << worst << '\n';
    } catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
