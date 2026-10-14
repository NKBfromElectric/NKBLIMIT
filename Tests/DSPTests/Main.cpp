// SPDX-License-Identifier: AGPL-3.0-only
#include "DSP/OversamplingEngine.h"
#include "ReferenceTruePeak.h"
#include <iostream>
#include <random>
#include <limits>
#include <chrono>
#include <atomic>
#include <cstdlib>
#include <new>
static std::atomic<std::size_t> allocations {0};
void* operator new(std::size_t n) { allocations.fetch_add(1); if (auto* p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc(); }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
void operator delete[](void* p,std::size_t) noexcept { std::free(p); }
static int checks = 0;
static double worstExcess = -100, largestRtf = 0;
void require(bool passed,const char* message) {
    ++checks; if (!passed) throw std::runtime_error(message);
}
template<typename T> void matrix() {
    for (double rate : {44100.,48000.,88200.,96000.,192000.}) {
        for (bool eight : {false,true}) {
          for (double ceilingDb : {-3.0,-0.5,0.0}) {
            for (int signal = 0; signal < 8; ++signal) {
                nkb::OversamplingEngine<T> engine;
                nkb::Parameters parameters {12.0,ceilingDb,eight};
                engine.prepare(rate,127,parameters);
                const int length = 4096 + engine.latency() + 128;
                std::vector<T> left(std::size_t(length),T{}),right(std::size_t(length),T{});
                std::mt19937 random(777);
                for (int i = 0; i < 4096; ++i) {
                    const double t = i/rate;
                    double value = 0.0;
                    switch (signal) {
                        case 0: break; // silence
                        case 1: value = 1.4; break; // DC
                        case 2: value = i == 1700 ? 5.0 : 0.0; break;
                        case 3: value = 1.7 * std::sin(2*std::numbers::pi*997*t); break;
                        case 4: value = 1.7 * std::sin(2*std::numbers::pi*rate*0.46*t+0.7); break;
                        case 5: value = std::sin(2*std::numbers::pi*(60*t+0.5*(rate*0.45)*t*t/(4096/rate))); break;
                        case 6: value = double(int(random()%20001)-10000)/5000.0; break;
                        case 7: value = (i%431 == 0 ? 20.0 : 0.2)*std::sin(2*std::numbers::pi*83*t); break;
                    }
                    left[std::size_t(i)] = T(value); right[std::size_t(i)] = T(value*0.37);
                }
                const auto allocatedBefore = allocations.load();
                const auto start = std::chrono::steady_clock::now();
                // Deliberately vary host block sizes, exceeding prepared capacity.
                int blockIndex = 0;
                for (int offset = 0; offset < length;) {
                    const int sizes[] {1,7,64,513,127,2048};
                    const int n = std::min(length-offset,sizes[blockIndex++%6]);
                    engine.process(left.data()+offset,right.data()+offset,n,parameters); offset += n;
                }
                const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
                largestRtf = std::max(largestRtf,elapsed/(length/rate));
                require(allocations.load() == allocatedBefore,"operator new allocation inside DSP process");
                std::vector<double> reference; reference.reserve(left.size());
                double peak = 0.0, ratioError = 0.0;
                for (int i = 0; i < length; ++i) {
                    require(std::isfinite(left[std::size_t(i)]) && std::isfinite(right[std::size_t(i)]),"non-finite output");
                    peak = std::max(peak,std::abs(double(left[std::size_t(i)])));
                    ratioError = std::max(ratioError,std::abs(double(right[std::size_t(i)])-double(left[std::size_t(i)])*0.37));
                    reference.push_back(double(left[std::size_t(i)]));
                }
                require(ratioError < (std::is_same_v<T,float> ? 3.0e-6 : 1.0e-10),"stereo ratio changed");
                if (signal == 0) require(peak == 0.0,"silence produced noise");
                const double tp = referenceTruePeak(reference);
                const double excess = nkb::gainToDb(tp)-parameters.ceilingDb;
                worstExcess = std::max(worstExcess,excess);
                if (excess > 0.1) std::cerr << "TP failure rate=" << rate << " signal=" << signal << " 8x=" << eight << " excess=" << excess << '\n';
                require(excess <= 0.1,"independent 16x reconstructed TP exceeded ceiling+0.1 dB");
            }
          }
        }
        std::cout << (std::is_same_v<T,float> ? "float" : "double") << " matrix rate=" << rate << " PASS\n" << std::flush;
    }
}
template<typename T> void robustness() {
    nkb::OversamplingEngine<T> engine;
    for (double rate : {44100.,48000.,192000.,96000.}) {
        engine.prepare(rate,16);
        std::vector<T> l(4096),r(4096);
        for (std::size_t i = 0; i < l.size(); ++i) { l[i] = T(std::sin(double(i)*0.05)*4); r[i] = T(std::cos(double(i)*0.03)*3); }
        l[2] = std::numeric_limits<T>::quiet_NaN(); r[3] = std::numeric_limits<T>::infinity();
        l[4] = -std::numeric_limits<T>::infinity(); r[5] = std::numeric_limits<T>::max();
        for (int i = 0; i < 4096; i += 32) {
            nkb::Parameters p {i%64 ? 24.0 : -12.0,i%64 ? 0.0 : -3.0,(i/32)%2 == 0};
            engine.process(l.data()+i,r.data()+i,32,p);
        }
        for (int i = 0; i < 4096; ++i) require(std::isfinite(l[std::size_t(i)]) && std::isfinite(r[std::size_t(i)]),"NaN/Inf spread");
    }
    // Below-threshold impulse: measured peak delay must match host latency.
    for (bool eight : {false,true}) {
        engine.prepare(48000,64,{0,-0.5,eight});
        std::vector<T> l(1024),r(1024); l[100] = r[100] = T(0.001);
        engine.process(l.data(),r.data(),1024,{0,-0.5,eight});
        auto it = std::max_element(l.begin(),l.end(),[](T a,T b) { return std::abs(a)<std::abs(b); });
        require(int(it-l.begin())-100 == engine.latency(),"reported latency disagrees with impulse delay");
    }
    // Partition invariance, including a sample-aligned oversampling switch.
    nkb::OversamplingEngine<T> a,b; a.prepare(48000,64); b.prepare(48000,64);
    std::vector<T> al(4096),ar(4096),bl(4096),br(4096);
    for (int i=0;i<4096;++i) al[std::size_t(i)] = ar[std::size_t(i)] = T(0.8*std::sin(i*0.08));
    bl=al; br=ar;
    for (int half=0;half<2;++half) {
        nkb::Parameters p {half ? 9.0 : 0.0,half ? -3.0 : -0.5,half != 0};
        a.process(al.data()+half*2048,ar.data()+half*2048,2048,p);
        for (int i=0;i<2048;i+=16) b.process(bl.data()+half*2048+i,br.data()+half*2048+i,16,p);
    }
    for (int i=0;i<4096;++i) require(std::abs(double(al[std::size_t(i)])-double(bl[std::size_t(i)])) < 1.0e-7,"block partition changed output");
    // Estimator/reference sine calibration at fs/4, with an intersample peak.
    std::vector<double> sine(4096);
    for (std::size_t i=0;i<sine.size();++i) sine[i]=0.9*std::sin(std::numbers::pi*0.5*i+std::numbers::pi/4);
    nkb::TruePeakMeter meter; double measured=0;
    for (double x:sine) measured=std::max(measured,meter.push(x,x));
    require(std::abs(nkb::gainToDb(measured)-nkb::gainToDb(referenceTruePeak(sine)))<0.15,"TP estimator disagrees with independent reference");
    // Low-level passband transparency, with the reported FIR delay compensated.
    for (bool eight : {false,true}) {
        engine.prepare(48000,127,{0,-0.5,eight});
        std::vector<T> l(8192),r(8192);
        for (int i=0;i<8192;++i) l[std::size_t(i)]=r[std::size_t(i)]=T(0.03*std::sin(i*2*std::numbers::pi*997/48000));
        engine.process(l.data(),r.data(),8192,{0,-0.5,eight});
        double squaredError=0,squaredReference=0;
        for (int i=2048;i<8192;++i) {
            const double expected=0.03*std::sin((i-engine.latency())*2*std::numbers::pi*997/48000);
            const double error=double(l[std::size_t(i)])-expected;
            squaredError+=error*error; squaredReference+=expected*expected;
        }
        std::cout << "passband 8x=" << eight << " error_ratio=" << squaredError/squaredReference << " latency=" << engine.latency() << '\n';
        // JUCE's integer-latency Thiran compensation has a small frequency-dependent
        // phase residual. Require <0.1% RMS error at this passband frequency.
        require(squaredError/squaredReference<1.0e-6,"low-level passband changed amplitude/phase excessively");
        const auto allocationCount=allocations.load(); engine.reset({0,-0.5,eight});
        require(allocations.load()==allocationCount,"DSP reset allocated");
    }
}
int main() {
    try {
        robustness<float>(); robustness<double>(); matrix<float>(); matrix<double>();
        std::cout << "PASS checks=" << checks << " worst_ceiling_excess_dB=" << worstExcess
                  << " largest_short_run_RTF=" << largestRtf << '\n';
        std::cout << "RTF includes both warm 4x/8x paths, safety FIR, meter; not a DAW CPU benchmark.\n";
    } catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
