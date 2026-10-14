// SPDX-License-Identifier: AGPL-3.0-only
#include "DSP/LimiterEngine.h"
#include <complex>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace {
void check(bool condition, const char* description) {
    if (!condition) throw std::runtime_error(description);
}

double harmonicDistortion(double rate, double frequency, double reductionDb) {
    nkb::LimiterEngine<double> engine;
    const auto ahead = static_cast<std::size_t>(std::ceil(rate * 0.0025));
    engine.prepare(rate, ahead);
    const double ceiling = nkb::dbToGain(-0.5);
    const double amplitude = ceiling * nkb::dbToGain(reductionDb);
    const int length = static_cast<int>(rate);
    std::vector<double> output(static_cast<std::size_t>(length));
    for (int i = 0; i < length * 4; ++i) {
        const double input = amplitude * std::sin(2.0 * std::numbers::pi * frequency * i / rate);
        const auto y = engine.process({input, 0.3 * input}, ceiling);
        if (i >= length * 3) output[static_cast<std::size_t>(i - length * 3)] = y[0];
    }
    double fundamental = 0.0, harmonics = 0.0;
    for (int harmonic = 1; harmonic <= 10; ++harmonic) {
        std::complex<double> sum {};
        for (int i = 0; i < length; ++i) {
            const double phase = -2.0 * std::numbers::pi * frequency * harmonic * i / rate;
            sum += output[static_cast<std::size_t>(i)] * std::complex<double>(std::cos(phase), std::sin(phase));
        }
        if (harmonic == 1) fundamental = std::norm(sum);
        else harmonics += std::norm(sum);
    }
    return std::sqrt(harmonics / fundamental);
}

void checkToneDistortion() {
    // Measure distortion after settling, independently from the gain detector.
    // These limits protect bass against an indiscriminately fast release.
    for (double rate : {48000.0, 384000.0}) {
        for (double reduction : {2.0, 6.0, 10.0}) {
            const double thd = harmonicDistortion(rate, 60.0, reduction);
            std::cout << "60 Hz rate=" << rate << " reduction=" << reduction << " THD=" << 100.0 * thd << "%\n";
            check(thd < 0.0016, "steady bass gain modulation exceeded 0.16% THD");
        }
        check(harmonicDistortion(rate, 30.0, 12.0) < 0.009, "30 Hz release distortion exceeded 0.9% THD");
        check(harmonicDistortion(rate, 997.0, 10.0) < 0.00001, "steady midrange tone acquired harmonic distortion");
    }
}

void checkTransientRecovery() {
    for (double rate : {44100.0, 48000.0, 96000.0, 384000.0}) {
        nkb::LimiterEngine<double> engine;
        const int ahead = static_cast<int>(std::ceil(rate * 0.0025));
        engine.prepare(rate, static_cast<std::size_t>(ahead));
        const double ceiling = nkb::dbToGain(-0.5);
        const int onset = static_cast<int>(rate), end = onset + static_cast<int>(0.060 * rate);
        int recovery = -1;
        for (int i = 0; i < static_cast<int>(1.5 * rate) + ahead; ++i) {
            const double age = (i - onset) / rate;
            const double bed = 0.10 * std::sin(2.0 * std::numbers::pi * 997.0 * i / rate);
            const double kick = i >= onset && i < end
                              ? 2.8 * std::exp(-50.0 * age) * std::sin(2.0 * std::numbers::pi * 60.0 * age) : 0.0;
            engine.process({bed + kick, bed + kick}, ceiling);
            if (i >= end + ahead && recovery < 0 && engine.lastGain >= nkb::dbToGain(-0.5))
                recovery = i - end - ahead;
        }
        const double milliseconds = 1000.0 * recovery / rate;
        std::cout << "kick rate=" << rate << " recovery-after-burst=" << milliseconds << " ms\n";
        check(recovery >= 0 && milliseconds < 60.0, "kick reduction continued ducking the quiet bed too long");

        engine.reset();
        for (int i = 0; i < static_cast<int>(rate); ++i) engine.process({0.05, 0.03}, ceiling);
        engine.process({4.0, 2.0}, ceiling);
        for (int i = 0; i < static_cast<int>(0.030 * rate) + ahead; ++i) engine.process({0.05, 0.03}, ceiling);
        check(engine.lastGain > nkb::dbToGain(-0.5), "isolated peak did not recover in 30 ms");
    }
}

void checkDriveMonotonicity() {
    constexpr double rate = 48000.0;
    constexpr int length = 48000 * 3, ahead = 120;
    const double ceiling = nkb::dbToGain(-0.5);
    std::vector<double> previous(static_cast<std::size_t>(length + ahead));
    double previousPower = 0.0;
    for (double drive : {4.0, 8.0, 12.0, 16.0, 20.0, 24.0}) {
        nkb::LimiterEngine<double> engine;
        engine.prepare(rate, ahead);
        double power = 0.0;
        for (int i = 0; i < length + ahead; ++i) {
            const double t = i / rate;
            const double kickAge = (i % 12000) / rate;
            const double snareAge = ((i + 6000) % 24000) / rate;
            const double kick = 0.8 * std::exp(-kickAge * 35.0) * std::sin(2.0 * std::numbers::pi * 60.0 * kickAge);
            const double snare = 0.4 * std::exp(-snareAge * 75.0)
                               * (std::sin(2.0 * std::numbers::pi * 731.0 * snareAge)
                                  + 0.5 * std::sin(2.0 * std::numbers::pi * 3209.0 * snareAge));
            const double bed = 0.12 * std::sin(2.0 * std::numbers::pi * 110.0 * t)
                             + 0.08 * std::sin(2.0 * std::numbers::pi * 997.0 * t);
            const double input = i < length ? (kick + snare + bed) * nkb::dbToGain(drive) : 0.0;
            const auto y = engine.process({input, input * 0.37}, ceiling);
            const double magnitude = std::abs(y[0]);
            check(magnitude + 1.0e-10 >= previous[static_cast<std::size_t>(i)],
                  "increasing input gain made the same synthetic music sample quieter");
            previous[static_cast<std::size_t>(i)] = magnitude;
            power += y[0] * y[0];
        }
        check(power + 1.0e-8 >= previousPower, "increasing drive reduced synthetic music RMS");
        std::cout << "synthetic drive=" << drive << " RMS=" << nkb::gainToDb(std::sqrt(power / length)) << " dBFS\n";
        previousPower = power;
    }
}

template <typename T> void checkBoundsAndReset() {
    std::mt19937 random(12891);
    for (double rate : {8000.0, 44100.0, 48000.0, 96000.0, 192000.0, 384000.0}) {
        for (std::size_t ahead : {std::size_t(0), std::size_t(1), static_cast<std::size_t>(std::ceil(rate * 0.0025))}) {
            nkb::LimiterEngine<T> engine;
            engine.prepare(rate, ahead);
            const double ceiling = nkb::dbToGain(-0.5);
            const double tolerance = std::is_same_v<T, float> ? 1.0e-7 : 1.0e-12;
            for (int i = 0; i < 12000; ++i) {
                const T value = T(static_cast<int>(random() % 20001) - 10000) / T(100);
                const auto y = engine.process({value, T(value * T(0.37))}, ceiling);
                check(std::isfinite(y[0]) && std::isfinite(y[1]), "non-finite bounded output");
                check(std::abs(double(y[0])) <= ceiling + tolerance, "left sample ceiling exceeded");
                check(std::abs(double(y[1])) <= ceiling + tolerance, "right sample ceiling exceeded");
                check(engine.lastGain >= 0.0 && engine.lastGain <= 1.0, "invalid linked gain");
            }
            engine.process({std::numeric_limits<T>::infinity(), std::numeric_limits<T>::quiet_NaN()}, ceiling);
            engine.reset();
            check(engine.lastGain == 1.0, "reset retained gain reduction");
            for (std::size_t i = 0; i < ahead + 100; ++i) {
                const auto y = engine.process({T(0), T(0)}, ceiling);
                check(y[0] == T(0) && y[1] == T(0) && engine.lastGain == 1.0, "reset retained delayed audio or detector state");
            }
            for (std::size_t i = 0; i < ahead + 2; ++i) {
                const auto y = engine.process({T(0.1), T(-0.05)}, ceiling);
                if (i < ahead) check(y[0] == T(0) && y[1] == T(0), "lookahead latency changed");
                else check(y[0] == T(0.1) && y[1] == T(-0.05), "unlimited signal changed after reset");
            }
        }
    }
}
} // namespace

int main() {
    try {
        checkBoundsAndReset<float>();
        checkBoundsAndReset<double>();
        checkTransientRecovery();
        checkDriveMonotonicity();
        checkToneDistortion();
        std::cout << "PASS limiter sample bound, finite handling, latency/reset, transient recovery, drive monotonicity, bass distortion\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
