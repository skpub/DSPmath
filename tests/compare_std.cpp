#include "dspmath.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

namespace
{
template <typename T>
struct ErrorStats
{
    T max_abs = 0;
    T max_rel = 0;
    T rms = 0;
};

template <typename T>
T relative_error(T actual, T expected)
{
    const T denominator = std::max<T>(std::abs(expected), T{1});
    return std::abs(actual - expected) / denominator;
}

template <typename T, typename DspFn, typename StdFn>
ErrorStats<T> compare_unary(const std::vector<T>& inputs, DspFn dsp_fn, StdFn std_fn)
{
    long double sum_squared = 0.0L;
    ErrorStats<T> stats{};

    for (const T input : inputs)
    {
        const T actual = dsp_fn(input);
        const T expected = std_fn(input);
        const T abs_error = std::abs(actual - expected);
        const T rel_error = relative_error(actual, expected);

        stats.max_abs = std::max(stats.max_abs, abs_error);
        stats.max_rel = std::max(stats.max_rel, rel_error);
        sum_squared += static_cast<long double>(abs_error) * static_cast<long double>(abs_error);
    }

    stats.rms = static_cast<T>(std::sqrt(sum_squared / static_cast<long double>(inputs.size())));
    return stats;
}

template <typename T>
std::vector<T> linspace(T first, T last, std::size_t count)
{
    std::vector<T> values;
    values.reserve(count);

    for (std::size_t i = 0; i < count; ++i)
    {
        const T t = static_cast<T>(i) / static_cast<T>(count - 1);
        values.push_back(first + (last - first) * t);
    }

    return values;
}

template <typename T, typename Fn>
double benchmark_ns_per_call(const std::vector<T>& inputs, Fn fn, std::size_t iterations)
{
    volatile T sink = 0;

    const auto started = std::chrono::steady_clock::now();
    for (std::size_t iter = 0; iter < iterations; ++iter)
    {
        for (const T input : inputs)
        {
            sink += fn(input);
        }
    }
    const auto ended = std::chrono::steady_clock::now();

    const auto elapsed_ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(ended - started).count();
    const auto calls = static_cast<double>(iterations * inputs.size());
    return static_cast<double>(elapsed_ns) / calls;
}

template <typename T, typename DspFn, typename StdFn>
bool check_unary(const std::string& name,
                 const std::vector<T>& inputs,
                 DspFn dsp_fn,
                 StdFn std_fn,
                 T max_abs_allowed,
                 T max_rel_allowed,
                 std::size_t benchmark_iterations)
{
    const auto stats = compare_unary(inputs, dsp_fn, std_fn);
    const double dsp_ns = benchmark_ns_per_call(inputs, dsp_fn, benchmark_iterations);
    const double std_ns = benchmark_ns_per_call(inputs, std_fn, benchmark_iterations);

    std::cout << std::left << std::setw(14) << name
              << " max_abs=" << std::scientific << stats.max_abs
              << " max_rel=" << stats.max_rel
              << " rms=" << stats.rms
              << " dsp_ns=" << std::fixed << std::setprecision(2) << dsp_ns
              << " std_ns=" << std_ns
              << " ratio=" << dsp_ns / std_ns << '\n';

    return stats.max_abs <= max_abs_allowed && stats.max_rel <= max_rel_allowed;
}

template <typename T>
bool check_pow(const std::vector<T>& bases,
               const std::vector<T>& exponents,
               T max_abs_allowed,
               T max_rel_allowed,
               std::size_t benchmark_iterations)
{
    std::vector<T> inputs;
    inputs.reserve(std::min(bases.size(), exponents.size()));

    ErrorStats<T> stats{};
    long double sum_squared = 0.0L;
    const std::size_t count = std::min(bases.size(), exponents.size());
    for (std::size_t i = 0; i < count; ++i)
    {
        const T actual = DSPmath::pow(bases[i], exponents[i]);
        const T expected = std::pow(bases[i], exponents[i]);
        const T abs_error = std::abs(actual - expected);
        const T rel_error = relative_error(actual, expected);

        stats.max_abs = std::max(stats.max_abs, abs_error);
        stats.max_rel = std::max(stats.max_rel, rel_error);
        sum_squared += static_cast<long double>(abs_error) * static_cast<long double>(abs_error);
    }
    stats.rms = static_cast<T>(std::sqrt(sum_squared / static_cast<long double>(count)));

    volatile T sink = 0;
    const auto benchmark = [&](auto fn) {
        const auto started = std::chrono::steady_clock::now();
        for (std::size_t iter = 0; iter < benchmark_iterations; ++iter)
        {
            for (std::size_t i = 0; i < count; ++i)
            {
                sink += fn(bases[i], exponents[i]);
            }
        }
        const auto ended = std::chrono::steady_clock::now();
        const auto elapsed_ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(ended - started).count();
        return static_cast<double>(elapsed_ns) /
               static_cast<double>(benchmark_iterations * count);
    };

    const double dsp_ns = benchmark([](T base, T exponent) { return DSPmath::pow(base, exponent); });
    const double std_ns = benchmark([](T base, T exponent) { return std::pow(base, exponent); });

    std::cout << std::left << std::setw(14) << (std::is_same_v<T, float> ? "pow(float)" : "pow(double)")
              << " max_abs=" << std::scientific << stats.max_abs
              << " max_rel=" << stats.max_rel
              << " rms=" << stats.rms
              << " dsp_ns=" << std::fixed << std::setprecision(2) << dsp_ns
              << " std_ns=" << std_ns
              << " ratio=" << dsp_ns / std_ns << '\n';

    return stats.max_abs <= max_abs_allowed && stats.max_rel <= max_rel_allowed;
}

template <typename T>
bool check_pow_fixed_base(const std::vector<T>& exponents,
                          T max_abs_allowed,
                          T max_rel_allowed,
                          std::size_t benchmark_iterations)
{
    constexpr T base = static_cast<T>(2);

    ErrorStats<T> stats{};
    long double sum_squared = 0.0L;
    for (const T exponent : exponents)
    {
        const T actual = DSPmath::pow(base, exponent);
        const T expected = std::pow(base, exponent);
        const T abs_error = std::abs(actual - expected);
        const T rel_error = relative_error(actual, expected);

        stats.max_abs = std::max(stats.max_abs, abs_error);
        stats.max_rel = std::max(stats.max_rel, rel_error);
        sum_squared += static_cast<long double>(abs_error) * static_cast<long double>(abs_error);
    }
    stats.rms = static_cast<T>(std::sqrt(sum_squared / static_cast<long double>(exponents.size())));

    const auto dsp_pow2 = [](T exponent) { return DSPmath::pow(base, exponent); };
    const auto std_pow2 = [](T exponent) { return std::pow(base, exponent); };
    const double dsp_ns = benchmark_ns_per_call(exponents, dsp_pow2, benchmark_iterations);
    const double std_ns = benchmark_ns_per_call(exponents, std_pow2, benchmark_iterations);

    std::cout << std::left << std::setw(14) << (std::is_same_v<T, float> ? "pow2(float)" : "pow2(double)")
              << " max_abs=" << std::scientific << stats.max_abs
              << " max_rel=" << stats.max_rel
              << " rms=" << stats.rms
              << " dsp_ns=" << std::fixed << std::setprecision(2) << dsp_ns
              << " std_ns=" << std_ns
              << " ratio=" << dsp_ns / std_ns << '\n';

    return stats.max_abs <= max_abs_allowed && stats.max_rel <= max_rel_allowed;
}

template <typename T>
bool run_type(const std::string& suffix)
{
    constexpr std::size_t sample_count = 4096;
    constexpr std::size_t benchmark_iterations = 1024;

    const auto trig_inputs =
        linspace<T>(static_cast<T>(-DSPmath::TAU * 8), static_cast<T>(DSPmath::TAU * 8), sample_count);
    const auto positive_inputs = linspace<T>(static_cast<T>(0.001), static_cast<T>(256), sample_count);
    const auto exp_inputs = linspace<T>(static_cast<T>(-10), static_cast<T>(10), sample_count);
    const auto pow_bases = linspace<T>(static_cast<T>(0.05), static_cast<T>(16), sample_count);
    const auto pow_exponents = linspace<T>(static_cast<T>(-3), static_cast<T>(3), sample_count);

    bool ok = true;

    const auto dsp_sin = [](T x) { return DSPmath::sin(x); };
    const auto dsp_cos = [](T x) { return DSPmath::cos(x); };
    const auto dsp_log = [](T x) { return DSPmath::log(x); };
    const auto dsp_log2 = [](T x) { return DSPmath::log2(x); };
    const auto dsp_exp = [](T x) { return DSPmath::exp(x); };
    const auto std_sin = [](T x) { return std::sin(x); };
    const auto std_cos = [](T x) { return std::cos(x); };
    const auto std_log = [](T x) { return std::log(x); };
    const auto std_log2 = [](T x) { return std::log2(x); };
    const auto std_exp = [](T x) { return std::exp(x); };

    if constexpr (std::is_same_v<T, float>)
    {
        // Tolerances reflect true 16-bit relative precision (rel = 2^-16 ~= 1.5e-5);
        // the absolute bound is rel * (max |output|) over each input range.
        constexpr float rel16 = 1.5e-5f;
        ok &= check_unary("sin(" + suffix + ")", trig_inputs, dsp_sin, std_sin, rel16, rel16, benchmark_iterations);
        ok &= check_unary("cos(" + suffix + ")", trig_inputs, dsp_cos, std_cos, rel16, rel16, benchmark_iterations);
        ok &= check_unary("log(" + suffix + ")", positive_inputs, dsp_log, std_log, 1.5e-4f, rel16, benchmark_iterations);
        ok &= check_unary("log2(" + suffix + ")", positive_inputs, dsp_log2, std_log2, 1.5e-4f, rel16, benchmark_iterations);
        ok &= check_unary("exp(" + suffix + ")", exp_inputs, dsp_exp, std_exp, 4.0e-1f, rel16, benchmark_iterations);
        ok &= check_pow(pow_bases, pow_exponents, 2.0e-1f, rel16, benchmark_iterations);
        ok &= check_pow_fixed_base(pow_exponents, 1.5e-4f, rel16, benchmark_iterations);
    }
    else
    {
        // Tolerances reflect true 24-bit relative precision (rel = 2^-24 ~= 6e-8);
        // the absolute bound is rel * (max |output|) over each input range.
        constexpr double rel24 = 6.0e-8;
        ok &= check_unary("sin(" + suffix + ")", trig_inputs, dsp_sin, std_sin, rel24, rel24, benchmark_iterations);
        ok &= check_unary("cos(" + suffix + ")", trig_inputs, dsp_cos, std_cos, rel24, rel24, benchmark_iterations);
        ok &= check_unary("log(" + suffix + ")", positive_inputs, dsp_log, std_log, 5.0e-7, rel24, benchmark_iterations);
        ok &= check_unary("log2(" + suffix + ")", positive_inputs, dsp_log2, std_log2, 6.0e-7, rel24, benchmark_iterations);
        ok &= check_unary("exp(" + suffix + ")", exp_inputs, dsp_exp, std_exp, 2.0e-3, rel24, benchmark_iterations);
        ok &= check_pow(pow_bases, pow_exponents, 1.0e-3, rel24, benchmark_iterations);
        ok &= check_pow_fixed_base(pow_exponents, 1.0e-6, rel24, benchmark_iterations);
    }

    return ok;
}
}

int main()
{
    static_assert(DSPmath::pimod(8.0) > 1.7168 && DSPmath::pimod(8.0) < 1.7169);
    static_assert(DSPmath::taumod(4.0) > 0.8584 && DSPmath::taumod(4.0) < 0.8585);
    static_assert(DSPmath::sin(DSPmath::PI / 6.0) > 0.499999 && DSPmath::sin(DSPmath::PI / 6.0) < 0.500001);
    static_assert(DSPmath::cos(DSPmath::PI / 3.0) > 0.499999 && DSPmath::cos(DSPmath::PI / 3.0) < 0.500001);
    static_assert(DSPmath::log2(8.0f) > 2.999f && DSPmath::log2(8.0f) < 3.001f);
    static_assert(DSPmath::log(1.0) > -0.000001 && DSPmath::log(1.0) < 0.000001);
    static_assert(DSPmath::exp(1.0f) > 2.718f && DSPmath::exp(1.0f) < 2.719f);
    static_assert(DSPmath::pow(2.0f, 3.0f) > 7.99f && DSPmath::pow(2.0f, 3.0f) < 8.01f);
    static_assert(DSPmath::pow(2.0, 3.0) > 7.999999 && DSPmath::pow(2.0, 3.0) < 8.000001);

    std::cout << "DSPmath vs std accuracy and timing\n";
    std::cout << "Timing is approximate and reported as nanoseconds per call.\n";

    const bool ok = run_type<float>("float") & run_type<double>("double");
    return ok ? 0 : 1;
}
