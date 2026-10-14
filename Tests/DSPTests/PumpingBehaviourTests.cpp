// SPDX-License-Identifier: AGPL-3.0-only
#include "DSP/LimiterEngine.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void check(bool condition, const char* description) {
    if (!condition) throw std::runtime_error(description);
}

double percentile(std::vector<double> values, double fraction) {
    check(!values.empty(), "no settled bed measurements");
    std::sort(values.begin(), values.end());
    const double index = fraction * static_cast<double>(values.size() - 1);
    const auto lo = static_cast<std::size_t>(index);
    const auto hi = std::min(lo + 1, values.size() - 1);
    return values[lo] + (index - static_cast<double>(lo)) * (values[hi] - values[lo]);
}

struct BedWindow {
    explicit BedWindow(int samples) : width(samples) {}
    void push(double source, double output) {
        sourcePower += source * source;
        outputPower += output * output;
        if (++count == width) {
            levels.push_back(10.0 * std::log10(outputPower / sourcePower));
            sourcePower = outputPower = 0.0;
            count = 0;
        }
    }
    double spread() const { return percentile(levels, 0.95) - percentile(levels, 0.05); }
    int width, count = 0;
    double sourcePower = 0.0, outputPower = 0.0;
    std::vector<double> levels;
};

template <typename T>
void checkPeriodicBed(double rate, double bpm, double drive, bool bass) {
    // A left kick must modulate a steady right-channel carrier because the
    // limiter is stereo linked. Measuring output/source power on that carrier
    // separates limiter pumping from the kick's own changing level. Relative
    // dB spread and normalised AM depth are independent of output-level matching.
    const int ahead = static_cast<int>(std::ceil(rate * 0.0025));
    const int period = static_cast<int>(std::round(rate * 60.0 / bpm));
    const int length = period * 24;
    const int first = period * 4, last = period * 22;
    const double ceiling = nkb::dbToGain(-0.5), factor = nkb::dbToGain(drive);
    nkb::LimiterEngine<T> limiter;
    limiter.prepare(rate, static_cast<std::size_t>(ahead));
    BedWindow shortWindow(static_cast<int>(std::round(rate * 0.050)));
    BedWindow slowWindow(static_cast<int>(std::round(rate * 0.400)));
    std::vector<double> gains(static_cast<std::size_t>(length));
    double cosine = 0.0, sine = 0.0, gainSum = 0.0;
    for (int i = 0; i < length + ahead; ++i) {
        const double t = i / rate, age = static_cast<double>(i % period) / rate;
        const double kick = i >= period * 2 && age < 0.120
                          ? 0.8 * std::exp(-35.0 * age) * std::sin(2.0 * std::numbers::pi * 60.0 * age) : 0.0;
        const double low = bass ? 0.12 * std::sin(2.0 * std::numbers::pi * 80.0 * t) : 0.0;
        const double bed = 0.06 * std::sin(2.0 * std::numbers::pi * 997.0 * t);
        const auto output = limiter.process({T(factor * (kick + low + 0.5 * bed)), T(factor * bed)}, ceiling);
        check(std::isfinite(output[0]) && std::isfinite(output[1]), "non-finite periodic bed output");
        check(std::max(std::abs(double(output[0])), std::abs(double(output[1]))) <= ceiling + 1.0e-7,
              "periodic kick exceeded sample ceiling");
        const int j = i - ahead;
        if (j < 0 || j >= length) continue;
        const double source = double(T(factor * 0.06 * std::sin(2.0 * std::numbers::pi * 997.0 * j / rate)));
        // At carrier zero crossings the output/source ratio is ill-conditioned.
        // The adjacent sample's gain is adequate for recovery timing; RMS windows
        // below still include every sample without special treatment.
        const double gain = std::abs(source) > 1.0e-8 ? std::abs(double(output[1]) / source)
                          : j > 0 ? nkb::dbToGain(gains[static_cast<std::size_t>(j - 1)]) : 1.0;
        gains[static_cast<std::size_t>(j)] = nkb::gainToDb(gain);
        if (j >= first && j < last) {
            shortWindow.push(source, double(output[1]));
            slowWindow.push(source, double(output[1]));
            const double phase = 2.0 * std::numbers::pi * j / period;
            cosine += gain * std::cos(phase);
            sine += gain * std::sin(phase);
            gainSum += gain;
        }
    }

    double recovery = 0.0;
    for (int cycle = 4; cycle < 22; ++cycle) {
        const int start = cycle * period, preCount = static_cast<int>(0.040 * rate);
        double preHit = 0.0;
        for (int j = start - preCount; j < start; ++j)
            preHit += gains[static_cast<std::size_t>(j)] / preCount;
        int returned = period;
        for (int j = static_cast<int>(0.120 * rate); j < period; ++j) {
            if (gains[static_cast<std::size_t>(start + j)] >= preHit - 0.5) { returned = j; break; }
        }
        recovery += 1000.0 * (returned - static_cast<int>(0.120 * rate)) / rate / 18.0;
    }
    const double amDepth = 200.0 * std::hypot(cosine, sine) / gainSum;
    const double shortSpread = shortWindow.spread(), slowSpread = slowWindow.spread();
    std::cout << "pumping rate=" << rate << " bpm=" << bpm << " drive=" << drive << " bass=" << bass
              << " bed-50ms-spread=" << shortSpread << " dB bed-400ms-spread=" << slowSpread
              << " dB AM=" << amDepth << "% recovery=" << recovery << " ms\n";

    // Limits protect measured short-window improvement over 0.1.2. A slower
    // average alone is insufficient: explicitly bound the modulation at the hit
    // repetition rate and the return time too. These are regression limits for
    // this signal, not an audibility or music-quality score.
    double maxShort = 0.0, maxSlow = 0.0;
    if (bpm == 120.0) {
        maxShort = drive == 7.0 ? (bass ? 1.7 : 1.8) : (bass ? 6.8 : 8.6);
        maxSlow = drive == 7.0 ? 0.5 : 1.4;
    } else {
        maxShort = drive == 7.0 ? (bass ? 2.4 : 1.3) : (bass ? 7.7 : 6.8);
        maxSlow = drive == 7.0 ? (bass ? 1.2 : 0.4) : (bass ? 3.2 : 1.1);
    }
    check(shortSpread < maxShort, "rapid stereo-linked bed modulation regressed");
    check(slowSpread < maxSlow, "slow stereo-linked bed modulation exceeded its bound");
    check(amDepth < (drive == 7.0 ? (bpm == 120.0 ? 6.0 : 6.5) : 30.0),
          "kick-rate amplitude modulation exceeded its bound");
    check(recovery < (drive == 7.0 ? 90.0 : bpm == 120.0 ? 280.0 : 180.0),
          "periodic kick held reduction for too long after its 120 ms burst");
}
} // namespace

int main() {
    try {
        for (double rate : {44100.0, 48000.0, 96000.0, 384000.0})
            for (double bpm : {120.0, 180.0})
                for (double drive : {7.0, 16.0})
                    for (bool bass : {false, true})
                        checkPeriodicBed<double>(rate, bpm, drive, bass);
        for (double bpm : {120.0, 180.0})
            for (double drive : {7.0, 16.0})
                for (bool bass : {false, true})
                    checkPeriodicBed<float>(48000.0, bpm, drive, bass);
        std::cout << "PASS linked held-bed modulation, periodic recovery, sample bounds at four rates and two drives\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
