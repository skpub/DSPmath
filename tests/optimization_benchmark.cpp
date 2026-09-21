// Override this macro to benchmark a saved pre-change header with the same harness.
#ifndef DSPMATH_BENCHMARK_HEADER
#define DSPMATH_BENCHMARK_HEADER "dspmath.hpp"
#endif
#include DSPMATH_BENCHMARK_HEADER
#include <algorithm>
#include <array>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

namespace {
volatile double sink;
constexpr int count = 4096, repeats = 256, rounds = 9;
std::array<double, count> inputs;
std::array<float, 16384> wavetable;

template<class F> double measure(F f, int mode) {
    double a = 0, b = 0, c = 0, d = 0;
    const auto start = std::chrono::steady_clock::now();
    for (int k = 0; k < repeats; ++k) {
        if (mode == 1) {
            // The scale/add overhead is common to both versions.
            for (double x : inputs) a = f(x + a * 0.00001);
        } else {
            for (int i = 0; i < count; i += 4) {
                if (mode == 2) {
                    // Synthetic 64 KiB competing table, not an application benchmark.
                    const auto j = (i * 137 + k * 31) & 16383;
                    a += f(inputs[i]) + wavetable[j];
                    b += f(inputs[i+1]) + wavetable[(j+4093)&16383];
                    c += f(inputs[i+2]) + wavetable[(j+8191)&16383];
                    d += f(inputs[i+3]) + wavetable[(j+12289)&16383];
                } else {
                    a += f(inputs[i]); b += f(inputs[i+1]);
                    c += f(inputs[i+2]); d += f(inputs[i+3]);
                }
            }
        }
    }
    sink = a+b+c+d;
    return std::chrono::duration<double,std::nano>(std::chrono::steady_clock::now()-start).count() /
           (count * repeats);
}
}

int main() {
    std::mt19937 rng(12345);
    std::uniform_real_distribution<double> dist(0.05,16);
    for (auto& x : inputs) x = dist(rng);
    for (auto& x : wavetable) x = float(dist(rng));
    std::vector<double> times[5][3];
    for (int round = -1; round < rounds; ++round) {
        for (int n = 0; n < 5; ++n) {
            const int i = (round % 2 == 0) ? 4-n : n;
            for (int mode = 0; mode < 3; ++mode) {
                double t = 0;
                switch (i) {
                case 0: t=measure([](double x){return DSPmath::log2(x);},mode);break;
                case 1: t=measure([](double x){return DSPmath::log(x);},mode);break;
                case 2: t=measure([](double x){return DSPmath::pow(x,1.7);},mode);break;
                case 3: t=measure([](double x){return DSPmath::sin(float(x));},mode);break;
                case 4: t=measure([](double x){return DSPmath::cos(float(x));},mode);break;
                }
                if (round >= 0) times[i][mode].push_back(t);
            }
        }
    }
    const char* names[] = {"log2(double)","log(double)","pow(double)","sin(float)","cos(float)"};
    const char* modes[] = {"independent","dependent","table_pressure"};
    std::cout << std::fixed << std::setprecision(3);
    for (int i=0;i<5;++i) for(int mode=0;mode<3;++mode) {
        auto& v=times[i][mode]; std::sort(v.begin(),v.end());
        std::cout << names[i] << ' ' << modes[mode] << " ns=" << v[rounds/2]
                  << " min=" << v.front() << " max=" << v.back() << '\n';
    }
}
