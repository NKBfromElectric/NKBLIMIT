// SPDX-License-Identifier: AGPL-3.0-only
#include "DSP/TruePeakGuard.h"
#include <iostream>
#include <random>
#include <stdexcept>
#include <type_traits>
#include <vector>

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

// Independent 20-phase, 128-tap beta=9 reconstruction. Production stages use
// beta=10 and different phase counts; this does not read their coefficients.
static double crosscheckTruePeak(const std::vector<double>& input, bool interiorOnly = false) {
    static const auto coefficients = [] {
        std::array<std::array<double,128>,20> h {};
        const auto i0 = [](double x) {
            double sum = 1., term = 1.;
            for (int k = 1; k < 40; ++k) { term *= x*x/(4.*k*k); sum += term; }
            return sum;
        };
        for (int p = 0; p < 20; ++p) {
            double total = 0.;
            for (int k = 0; k < 128; ++k) {
                const double t = k - 63. - double(p)/20.;
                const double sinc = std::abs(t) < 1.e-12 ? 1. : std::sin(std::numbers::pi*t)/(std::numbers::pi*t);
                const double r = 2.*k/127.-1.;
                h[p][k] = sinc*i0(9.*std::sqrt(std::max(0.,1.-r*r)))/i0(9.);
                total += h[p][k];
            }
            for (auto& value : h[p]) value /= total;
        }
        return h;
    }();
    double maximum = 0.;
    for (std::size_t n = 0; n < input.size()+128; ++n) for (const auto& h : coefficients) {
        // A cropped steady-state excerpt must not acquire fictitious hard
        // cuts at its boundaries. Burst tests include the genuine full tail.
        if (interiorOnly && (n < 128 || n >= input.size())) continue;
        double value = 0.;
        for (std::size_t k = 0; k < 128; ++k)
            if (n >= k && n-k < input.size()) value += h[k]*input[n-k];
        maximum = std::max(maximum,std::abs(value));
    }
    return maximum;
}

static double worstHeadroom = 0., worstBurstExcess = -240., maximumScalarError = 0.;
static int crosscheckCases = 0;

template<typename T> static void steadyTracking() {
    constexpr double rates[] {44100.,48000.,88200.,96000.,192000.};
    constexpr double ceilings[] {-3.,-.5,0.};
    for (const double rate : rates) for (const double ceilingDb : ceilings) {
        nkb::TruePeakGuard<T> guard; guard.prepare(rate);
        std::vector<double> output(8192);
        const double phaseStep = 2.*std::numbers::pi*997./rate;
        for (int i = 0; i < 16384; ++i) {
            const auto y = guard.process({T(2.*std::sin(phaseStep*i)),T{}},nkb::dbToGain(ceilingDb));
            if (i >= 8192) output[std::size_t(i-8192)] = double(y[0]);
        }
        const double peakDb = nkb::gainToDb(crosscheckTruePeak(output,true));
        const double headroom = ceilingDb-peakDb;
        worstHeadroom = std::max(worstHeadroom,headroom);
        require(headroom <= .11,"steady tone remains unnecessarily below requested ceiling");
        require(peakDb <= ceilingDb+.1,"steady tone crossed independent reconstruction ceiling");
        ++crosscheckCases;
    }
}

template<typename T> static void modulationCrosscheck() {
    constexpr double rates[] {44100.,48000.,96000.,192000.};
    constexpr double amplitudes[] {1.,4.,20.,200.};
    constexpr int length = 4096, tail = nkb::TruePeakGuard<T>::latency + 128;
    for (const double rate : rates) for (const double amplitude : amplitudes) {
        nkb::TruePeakGuard<T> guard; guard.prepare(rate);
        std::vector<double> left(length+tail), right(length+tail);
        for (int i = 0; i < length+tail; ++i) {
            double l = 0., r = 0.;
            if (i < length) {
                // Near-Nyquist bursts challenge both gain modulation and the
                // different reference-filter transition bands.
                l = (i > 1700 && i < 2000 ? amplitude : .07)*std::sin(2.*std::numbers::pi*.499*i+.817);
                r = amplitude*std::cos(.121*i);
            }
            const auto y = guard.process({T(l),T(r)},nkb::dbToGain(-.5));
            left[std::size_t(i)] = double(y[0]); right[std::size_t(i)] = double(y[1]);
        }
        const double excess = nkb::gainToDb(std::max(crosscheckTruePeak(left),crosscheckTruePeak(right)))+.5;
        worstBurstExcess = std::max(worstBurstExcess,excess);
        require(excess <= .1,"gain-modulated burst crossed 20x128 reconstruction ceiling");
        ++crosscheckCases;
    }
}

template<int phases> static void scalarAgreement() {
    nkb::detail::ReconstructionGuardStage<double,phases,true> vectorStage(.02);
    nkb::detail::ReconstructionGuardStage<double,phases,false> scalarStage(.02);
    vectorStage.prepare(192000.); scalarStage.prepare(192000.);
    std::mt19937 random(83178);
    for (int i = 0; i < 32768; ++i) {
        const double amplitude = i%1700 < 300 ? 200. : 2.;
        const std::array<double,2> input {
            amplitude*double(int(random()%20001)-10000)/10000.,
            amplitude*std::sin(2.*std::numbers::pi*.499*i)
        };
        const double ceiling = nkb::dbToGain(i%6000 < 3000 ? -.5 : -3.);
        const auto vectorOutput = vectorStage.process(input,ceiling);
        const auto scalarOutput = scalarStage.process(input,ceiling);
        const double error = std::max({std::abs(vectorOutput[0]-scalarOutput[0]),
            std::abs(vectorOutput[1]-scalarOutput[1]),std::abs(vectorStage.gain-scalarStage.gain)});
        maximumScalarError = std::max(maximumScalarError,error);
        require(error <= 1.e-10,"vector sum changes reconstruction guard beyond double roundoff");
    }
}

template<typename T> static void alignedReduction() {
    nkb::TruePeakGuard<T> guard; guard.prepare(48000.);
    std::vector<std::array<T,2>> input(12288);
    for (std::size_t i = 0; i < input.size(); ++i) {
        const double amplitude = i%1000 < 240 ? 4. : .3;
        input[i] = {T(amplitude*std::sin(.17*i)),T(amplitude*std::cos(.121*i))};
        const auto y = guard.process(input[i],nkb::dbToGain(-.5));
        if (i >= std::size_t(nkb::TruePeakGuard<T>::latency)) {
            const auto original = input[i-std::size_t(nkb::TruePeakGuard<T>::latency)];
            const double error = std::max(std::abs(double(y[0])-double(original[0])*guard.gain),
                std::abs(double(y[1])-double(original[1])*guard.gain));
            require(error < (std::is_same_v<T,float> ? 3.e-6 : 1.e-12),"reported reduction is not aligned to output audio");
        }
    }
}

int main() {
    try {
        steadyTracking<float>(); steadyTracking<double>();
        modulationCrosscheck<float>(); modulationCrosscheck<double>();
        scalarAgreement<16>(); scalarAgreement<8>();
        alignedReduction<float>(); alignedReduction<double>();
        std::cout << "PASS CeilingTracking crosscheck_cases=" << crosscheckCases
            << " worst_steady_headroom_dB=" << worstHeadroom
            << " worst_burst_excess_dB=" << worstBurstExcess
            << " max_vector_scalar_error=" << maximumScalarError << '\n';
    } catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
