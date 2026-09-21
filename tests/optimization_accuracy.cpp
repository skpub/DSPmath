#include "dspmath.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>

namespace {
struct Error {
    double maximum = 0;
    bool finite = true;
    void check(double actual, double expected) {
        finite &= std::isfinite(actual) && std::isfinite(expected);
        maximum = std::max(maximum, std::abs(actual - expected) /
                                      std::max(1.0, std::abs(expected)));
    }
    bool report(const char* name, double tolerance) const {
        std::cout << name << " max_scaled_error=" << maximum << '\n';
        return finite && maximum <= tolerance;
    }
};
}

int main() {
    static_assert(DSPmath::log2(8.0) == 3.0);
    static_assert(DSPmath::sin(0.0f) == 0.0f);
    Error log_error, ln_error, pow_error, sin_error, cos_error;
    auto check_log = [&](double x) {
        log_error.check(DSPmath::log2(x), std::log2(x));
        ln_error.check(DSPmath::log(x), std::log(x));
    };
    auto check_trig = [&](float x) {
        sin_error.check(DSPmath::sin(x), std::sin(double(x)));
        cos_error.check(DSPmath::cos(x), std::cos(double(x)));
    };
    // Mantissa sweep, including cancellation near x=1 and varying exponents.
    for (int i = 0; i <= 200000; ++i) {
        const double m = 1.0 + double(i) / 200000;
        check_log(m);
        check_log(m * 0.5);
        check_log(std::ldexp(m, i % 2001 - 1000));
        check_trig(float(-8 * DSPmath::TAU + 16 * DSPmath::TAU * i / 200000));
    }
    // Both sides of every log-table boundary over representative exponents.
    for (int e : {-1022, -100, -1, 0, 1, 100, 1022}) {
        for (int i = 0; i <= 64; ++i) {
            const double x = std::ldexp(1.0 + double(i) / 64, e);
            check_log(x);
            const double below = std::nextafter(x, 0.0);
            if (below >= std::numeric_limits<double>::min()) check_log(below);
            check_log(std::nextafter(x, std::numeric_limits<double>::infinity()));
        }
    }
    check_log(std::numeric_limits<double>::min());
    check_log(std::numeric_limits<double>::max());
    // Independent base/exponent grid; wider than compare_std's diagonal.
    for (int i = 0; i <= 512; ++i) {
        const double base = 0.05 + 15.95 * i / 512;
        for (int j = 0; j <= 256; ++j) {
            const double exponent = -64.0 + 128.0 * j / 256;
            pow_error.check(DSPmath::pow(base, exponent), std::pow(base, exponent));
        }
    }
    // Negative and positive LUT boundaries, plus bias rounding transitions.
    for (int i = -2048; i <= 2048; ++i) {
        for (double offset : {0.0, 0.25, 0.5, 0.75}) {
            const float x = float((i + offset) * DSPmath::TAU / 256);
            check_trig(x);
            check_trig(std::nextafter(x, std::numeric_limits<float>::infinity()));
            check_trig(std::nextafter(x, -std::numeric_limits<float>::infinity()));
        }
    }
    const bool ok = log_error.report("log2(double)", 6e-8) &
                    ln_error.report("log(double)", 6e-8) &
                    pow_error.report("pow(double)", 6e-8) &
                    sin_error.report("sin(float)", 1.5e-5) &
                    cos_error.report("cos(float)", 1.5e-5);
    return ok ? 0 : 1;
}
