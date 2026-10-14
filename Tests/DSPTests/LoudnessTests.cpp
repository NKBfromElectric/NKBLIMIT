// SPDX-License-Identifier: AGPL-3.0-only
// Standalone C++20 tests; no JUCE dependency.
#include "Metering/LoudnessMeter.h"
#include <atomic>
#include <chrono>
#include <complex>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <numbers>
#include <stdexcept>
#include <vector>
#if defined(_MSC_VER)
#include <malloc.h>
#endif

static std::atomic<std::size_t> allocationCount {0};
void* operator new(std::size_t bytes) {
    allocationCount.fetch_add(1, std::memory_order_relaxed);
    if (auto* memory = std::malloc(bytes ? bytes : 1)) return memory;
    throw std::bad_alloc();
}
void* operator new[](std::size_t bytes) { return ::operator new(bytes); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }
void* operator new(std::size_t bytes, std::align_val_t alignment) {
    allocationCount.fetch_add(1, std::memory_order_relaxed);
    const auto align = static_cast<std::size_t>(alignment);
#if defined(_MSC_VER)
    void* memory = _aligned_malloc(bytes ? bytes : 1, align);
#else
    void* memory = std::aligned_alloc(align, ((bytes ? bytes : 1) + align - 1) / align * align);
#endif
    if (memory) return memory;
    throw std::bad_alloc();
}
void* operator new[](std::size_t bytes, std::align_val_t alignment) {
    return ::operator new(bytes, alignment);
}
void operator delete(void* memory, std::align_val_t) noexcept {
#if defined(_MSC_VER)
    _aligned_free(memory);
#else
    std::free(memory);
#endif
}
void operator delete[](void* memory, std::align_val_t alignment) noexcept {
    ::operator delete(memory, alignment);
}
void operator delete(void* memory, std::size_t, std::align_val_t alignment) noexcept {
    ::operator delete(memory, alignment);
}
void operator delete[](void* memory, std::size_t, std::align_val_t alignment) noexcept {
    ::operator delete(memory, alignment);
}

namespace {
int checks = 0;
void require(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
void near(double actual, double expected, double tolerance, const char* message) {
    if (std::abs(actual - expected) > tolerance || !std::isfinite(actual))
        std::cerr << message << ": actual=" << actual << " expected=" << expected << '\n';
    require(std::isfinite(actual) && std::abs(actual - expected) <= tolerance, message);
}

// Independent direct-form-I reference using the normative 48 kHz tables.
// Production uses transposed direct form II and a sum tree.
struct ReferenceBiquad {
    long double b0, b1, b2, a1, a2;
    long double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    long double push(long double x) noexcept {
        const long double y = b0*x + b1*x1 + b2*x2 - a1*y1 - a2*y2;
        x2 = x1; x1 = x; y2 = y1; y1 = y;
        return y;
    }
};
struct Reference48k {
    ReferenceBiquad shelf[2] {
        {1.53512485958697L, -2.69169618940638L, 1.19839281085285L,
         -1.69065929318241L, 0.73248077421585L},
        {1.53512485958697L, -2.69169618940638L, 1.19839281085285L,
         -1.69065929318241L, 0.73248077421585L}
    };
    ReferenceBiquad highPass[2] {
        {1.0L, -2.0L, 1.0L, -1.99004745483398L, 0.99007225036621L},
        {1.0L, -2.0L, 1.0L, -1.99004745483398L, 0.99007225036621L}
    };
    std::vector<long double> energies = std::vector<long double>(144000, 0.0L);
    std::size_t position = 0;
    void push(double left, double right) {
        const long double l = highPass[0].push(shelf[0].push(left));
        const long double r = highPass[1].push(shelf[1].push(right));
        energies[position] = l*l + r*r;
        position = (position + 1) % energies.size();
    }
    double loudness() const {
        // Full recomputation has no production rolling accumulator/tree code.
        long double power = 0.0L;
        for (const auto energy : energies) power += energy;
        return power > 0 ? std::max(-240.0, -0.691 + 10.0*std::log10(
            static_cast<double>(power / static_cast<long double>(energies.size())))) : -240.0;
    }
};

std::complex<double> referenceTransfer(double omega) {
    const auto z = std::polar(1.0, -omega);
    const auto shelf = (1.53512485958697 - 2.69169618940638*z + 1.19839281085285*z*z)
                     / (1.0 - 1.69065929318241*z + 0.73248077421585*z*z);
    const auto highPass = (1.0 - 2.0*z + z*z)
                        / (1.0 - 1.99004745483398*z + 0.99007225036621*z*z);
    return shelf * highPass;
}
double expectedStereoSine(double rate, double frequency, double amplitude) {
    // Evaluate H48 at the frequency implied by the inverse/forward bilinear
    // transform, without deriving/reusing production coefficient polynomials.
    const double referenceOmega = 2.0 * std::atan((rate/48000.0)
                                  * std::tan(std::numbers::pi*frequency/rate));
    // Stereo equal sine: 2 channels * A^2/2 = A^2. L/R phase is irrelevant.
    return -0.691 + 10.0*std::log10(amplitude*amplitude
                                   * std::norm(referenceTransfer(referenceOmega)));
}
void feedSine(nkb::LoudnessMeter& meter, double rate, double seconds,
              double frequency, double amplitude, bool rightEnabled = true,
              bool reverseRight = false) {
    const auto samples = static_cast<std::size_t>(std::llround(rate*seconds));
    for (std::size_t i = 0; i < samples; ++i) {
        const double value = amplitude*std::sin(2.0*std::numbers::pi*frequency
                                               * static_cast<double>(i)/rate);
        meter.push(value, rightEnabled ? (reverseRight ? -value : value) : 0.0);
    }
}

void referenceAndSlidingWindow() {
    nkb::LoudnessMeter meter;
    meter.prepare(48000.0);
    Reference48k reference;
    for (int i = 0; i < 48000*8; ++i) {
        const double t = static_cast<double>(i)/48000.0;
        const double scale = i < 48000 ? 0.8 : (i < 48000*4 ? 0.025 : 0.0);
        const double left = scale*(std::sin(2.0*std::numbers::pi*997.0*t)
                            + 0.15*std::sin(2.0*std::numbers::pi*83.0*t));
        const double right = scale*0.37*std::sin(2.0*std::numbers::pi*3127.0*t + 0.9);
        meter.push(left, right); reference.push(left, right);
        if (i % 4800 == 4799)
            near(meter.shortTermLufs(), reference.loudness(), 2.0e-7,
                 "48k normative direct-form-I / exact sliding window");
    }
}

void sampleRateAndCalibration() {
    for (const double rate : {44100.0, 48000.0, 88200.0, 96000.0, 192000.0}) {
        nkb::LoudnessMeter meter;
        meter.prepare(rate);
        for (const double frequency : {40.0, 997.0, 4000.0, 15000.0}) {
            meter.reset();
            feedSine(meter, rate, 5.0, frequency, 0.1);
            near(meter.shortTermLufs(), expectedStereoSine(rate, frequency, 0.1),
                 1.0e-6, "steady sine agrees with independent frequency response");
            require(meter.isReady(), "5-second tone readiness");
        }
        meter.reset();
        feedSine(meter, rate, 5.0, 997.0, 0.1);
        near(meter.shortTermLufs(), -20.0, 0.025, "997 Hz stereo -20 dBFS calibration");
        std::cout << "LUFS sine calibration / frequency response rate=" << rate << " PASS\n";
    }
}

void channelWeightsAndPhase() {
    nkb::LoudnessMeter stereo, single, reverse;
    stereo.prepare(48000.0); single.prepare(48000.0); reverse.prepare(48000.0);
    feedSine(stereo, 48000.0, 5.0, 997.0, 0.1);
    feedSine(single, 48000.0, 5.0, 997.0, 0.1, false);
    feedSine(reverse, 48000.0, 5.0, 997.0, 0.1, true, true);
    near(stereo.shortTermLufs() - single.shortTermLufs(), 10.0*std::log10(2.0),
         1.0e-10, "L/R weights 1.0, no stereo power averaging");
    near(stereo.shortTermLufs(), reverse.shortTermLufs(), 1.0e-10,
         "opposite-polarity stereo must not cancel in loudness measurement");
}

void startupDecayAndReset() {
    nkb::LoudnessMeter meter;
    require(!meter.isReady(), "unprepared meter readiness");
    meter.push(1.0, 1.0);
    near(meter.shortTermLufs(), -240.0, 0.0, "unprepared floor");
    for (const double rate : {44100.0, 48000.0, 88200.0, 96000.0, 192000.0}) {
        meter.prepare(rate);
        const auto window = static_cast<std::size_t>(std::llround(rate*3.0));
        for (std::size_t i = 0; i < window-1; ++i) meter.push(0.0, 0.0);
        require(!meter.isReady(), "readiness must not precede 3-second window");
        meter.push(0.0, 0.0);
        require(meter.isReady(), "readiness at exactly 3-second window");
        near(meter.shortTermLufs(), -240.0, 0.0, "ready silence floor");
    }
    meter.prepare(48000.0);
    feedSine(meter, 48000.0, 1.0, 997.0, 0.1);
    near(meter.shortTermLufs(), expectedStereoSine(48000.0, 997.0, 0.1)
                              + 10.0*std::log10(1.0/3.0), 0.003,
         "startup uses 3-second zero-padded denominator");
    feedSine(meter, 48000.0, 5.0, 997.0, 0.1);
    const double full = meter.shortTermLufs();
    for (int i = 0; i < 72000; ++i) meter.push(0.0, 0.0);
    near(meter.shortTermLufs() - full, 10.0*std::log10(0.5), 0.02,
         "rectangular window half-tone / half-silence decay");
    for (int i = 0; i < 48000*5; ++i) meter.push(0.0, 0.0);
    near(meter.shortTermLufs(), -240.0, 0.0,
         "silence recovery without residual accumulator energy");

    // Reset against a fresh instance for startup AND a complete ring wrap.
    feedSine(meter, 48000.0, 4.0, 997.0, 1.0e6);
    nkb::LoudnessMeter fresh; fresh.prepare(48000.0);
    meter.reset();
    require(!meter.isReady(), "reset clears readiness");
    near(meter.shortTermLufs(), -240.0, 0.0, "reset clears loudness immediately");
    for (int i = 0; i < 145001; ++i) {
        const double value = i % 241 == 0 ? 0.3 : 0.0;
        meter.push(value, -0.71*value); fresh.push(value, -0.71*value);
        if (i < 33 || i % 1441 == 0)
            near(meter.shortTermLufs(), fresh.shortTermLufs(), 0.0,
                 "lazy reset tree has no stale history");
    }
}

void sanitisationAndAllocations() {
    nkb::LoudnessMeter bad, clean;
    bad.prepare(48000.0); clean.prepare(48000.0);
    const double raw[] {std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::max(), -std::numeric_limits<double>::max(),
        std::numeric_limits<double>::denorm_min(), 0.0, 0.031};
    const auto allocationsBefore = allocationCount.load();
    for (int i = 0; i < 48000*4; ++i) {
        const double input = raw[i % 8];
        const double sanitised = std::isfinite(input) ? std::clamp(input, -1.0e6, 1.0e6) : 0.0;
        bad.push(input, -input); clean.push(sanitised, -sanitised);
        if (i % 127 == 0) {
            near(bad.shortTermLufs(), clean.shortTermLufs(), 0.0,
                 "NaN / infinity / outlier sanitisation");
            require(std::isfinite(bad.shortTermLufs()), "finite loudness under hostile samples");
        }
    }
    for (int reset = 0; reset < 31; ++reset) {
        bad.reset();
        for (int i = 0; i < 1009; ++i) bad.push(0.03, -0.02);
        require(std::isfinite(bad.shortTermLufs()) && !bad.isReady(),
                "allocation test reset/startup validity");
    }
    require(allocationCount.load() == allocationsBefore,
            "allocation inside push, reset, getter or readiness query");
    std::cout << "LUFS callbacks and reset: zero operator new allocations PASS\n";
    bad.prepare(std::numeric_limits<double>::quiet_NaN());
    clean.prepare(48000.0);
    bad.push(0.2, 0.1); clean.push(0.2, 0.1);
    near(bad.shortTermLufs(), clean.shortTermLufs(), 0.0, "invalid rate fallback to 48k");
}
} // namespace

int main() {
    try {
        const auto start = std::chrono::steady_clock::now();
        referenceAndSlidingWindow();
        sampleRateAndCalibration();
        channelWeightsAndPhase();
        startupDecayAndReset();
        sanitisationAndAllocations();
        const double seconds = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - start).count();
        std::cout << "LoudnessTests PASS: " << checks << " checks, " << seconds << " seconds\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "LoudnessTests FAIL: " << error.what() << '\n';
        return 1;
    }
}
