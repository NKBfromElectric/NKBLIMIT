// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace nkb {
// Stereo, ungated Short-term loudness: K weighting + a 3 s rectangular window.
// BS.1770-5 Annex 1 filter/power calculation; EBU Tech 3341 short-term interval.
// This component alone does not imply complete EBU Mode or certified compliance.
// Single audio-thread owner. Publish its results separately with atomics.
class LoudnessMeter {
public:
    static constexpr double silenceLufs = -240.0;

    // Lifecycle call: may allocate. Invalid rates use 48 kHz; supported operating
    // range is 8--384 kHz. See docs/loudness-design.md for coefficient mapping.
    void prepare(double sampleRate) {
        const double rate = std::isfinite(sampleRate) && sampleRate > 0.0
                          ? std::clamp(sampleRate, 8000.0, 384000.0) : 48000.0;
        windowSamples = static_cast<std::size_t>(std::llround(3.0 * rate));
        leafCount = 1;
        while (leafCount < windowSamples) leafCount *= 2;
        energyTree.resize(leafCount * 2);
        const auto shelf = mapFrom48k({1.53512485958697, -2.69169618940638,
                                      1.19839281085285, -1.69065929318241,
                                      0.73248077421585}, rate);
        const auto highPass = mapFrom48k({1.0, -2.0, 1.0, -1.99004745483398,
                                         0.99007225036621}, rate);
        for (auto& channel : filters) {
            channel[0].coefficients = shelf;
            channel[1].coefficients = highPass;
        }
        reset();
    }

    // O(1), no allocation and no clearing of a multi-megabyte history. After
    // reset, the tree's overwritten prefix is valid; every other leaf is zero.
    void reset() noexcept {
        for (auto& channel : filters)
            for (auto& filter : channel) filter.reset();
        position = 0;
        samplesSeen = 0;
    }

    void push(double left, double right) noexcept {
        if (windowSamples == 0) return;
        const double l = filters[0][1].push(filters[0][0].push(sanitise(left)));
        const double r = filters[1][1].push(filters[1][0].push(sanitise(right)));
        // BS.1770 stereo weights are both 1. Sum channel powers; do not sum
        // channels first (which would introduce phase-dependent cancellation).
        const double energy = l * l + r * r;
        if (samplesSeen < windowSamples) ++samplesSeen;
        std::size_t node = leafCount + position;
        energyTree[node] = energy;
        std::size_t rangeStart = position;
        std::size_t width = 1;
        while (node > 1) {
            node /= 2;
            rangeStart &= ~(2 * width - 1);
            const std::size_t rightStart = rangeStart + width;
            // The left child contains valid samples because it is in the
            // already-written prefix or on this sample's path. The right child
            // may contain stale values from before reset, or unused tail leaves.
            const double rightEnergy = rightStart < samplesSeen
                                     ? energyTree[2 * node + 1] : 0.0;
            energyTree[node] = energyTree[2 * node] + rightEnergy;
            width *= 2;
        }
        if (++position == windowSamples) position = 0;
    }

    [[nodiscard]] double shortTermLufs() const noexcept {
        if (samplesSeen == 0) return silenceLufs;
        const double meanSquare = energyTree[1] / static_cast<double>(windowSamples);
        if (!(meanSquare > 0.0) || !std::isfinite(meanSquare)) return silenceLufs;
        return std::max(silenceLufs, -0.691 + 10.0 * std::log10(meanSquare));
    }

    [[nodiscard]] bool isReady() const noexcept {
        return windowSamples != 0 && samplesSeen == windowSamples;
    }

private:
    struct Coefficients { double b0, b1, b2, a1, a2; };
    struct Biquad {
        Coefficients coefficients {1.0, 0.0, 0.0, 0.0, 0.0};
        double z1 = 0.0, z2 = 0.0;
        void reset() noexcept { z1 = z2 = 0.0; }
        static double flushTiny(double value) noexcept {
            return std::abs(value) < 1.0e-30 ? 0.0 : value;
        }
        double push(double input) noexcept {
            const auto& c = coefficients;
            const double output = c.b0 * input + z1;
            z1 = flushTiny(c.b1 * input - c.a1 * output + z2);
            z2 = flushTiny(c.b2 * input - c.a2 * output);
            return flushTiny(output);
        }
    };

    static double sanitise(double value) noexcept {
        return std::isfinite(value) ? std::clamp(value, -1.0e6, 1.0e6) : 0.0;
    }

    static Coefficients mapFrom48k(Coefficients reference, double rate) noexcept {
        // Preserve the normative coefficients verbatim at their reference rate.
        if (rate == 48000.0) return reference;
        // Inverse bilinear transform at 48k, then forward transform at rate.
        // If q=(1-z^-1)/(1+z^-1), q_reference=(rate/48000)*q_new.
        const double k = rate / 48000.0;
        const auto mapPolynomial = [k](double c0, double c1, double c2) {
            const double p0 = c0 + c1 + c2;
            const double p1 = 2.0 * (c0 - c2) * k;
            const double p2 = (c0 - c1 + c2) * k * k;
            return std::array<double, 3> {p0 + p1 + p2,
                                         2.0 * (p0 - p2), p0 - p1 + p2};
        };
        const auto numerator = mapPolynomial(reference.b0, reference.b1, reference.b2);
        const auto denominator = mapPolynomial(1.0, reference.a1, reference.a2);
        return {numerator[0] / denominator[0], numerator[1] / denominator[0],
                numerator[2] / denominator[0], denominator[1] / denominator[0],
                denominator[2] / denominator[0]};
    }

    std::array<std::array<Biquad, 2>, 2> filters {};
    std::vector<double> energyTree;
    std::size_t windowSamples = 0, leafCount = 0, position = 0, samplesSeen = 0;
};
} // namespace nkb
