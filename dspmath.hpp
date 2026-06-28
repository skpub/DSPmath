#pragma once

/*
 ==============================================================================

 This file is part of DSPmath.

 Copyright (c) 2026 Kaito Sato <satodeyannsu@gmail.com>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.

 ==============================================================================
*/

#include <cmath>
#include <cstdint>
#include <array>
#include <bit>
#include <cstddef>
#include <limits>
#include <type_traits>

#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#include <immintrin.h>
#define DSPMATH_HAS_SSE2 1
#endif

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#define DSPMATH_HAS_NEON 1
#if defined(__aarch64__) || defined(_M_ARM64)
#define DSPMATH_HAS_NEON_F32_DIV 1
#endif
#endif

namespace DSPmath
{
    constexpr double PI = 3.1415926535897932384626433832795028841971;
    constexpr double INV_PI = 1.0f / PI;
    constexpr double TAU = 2.0 * PI;
    constexpr double INV_TAU = 1.0f / TAU;
    constexpr double LN2 =
        0.693147180559945309417232121458176568;
    constexpr double INV_LN2 =
        1.442695040888963407359924681001892137;

    constexpr double LOG2E =
        1.44269504088896340736;

    namespace detail
    {
        template <typename T>
        constexpr T abs(T x)
        {
            return x < static_cast<T>(0) ? -x : x;
        }

        template <typename T>
        constexpr T trunc(T x)
        {
            if (std::is_constant_evaluated())
            {
                return static_cast<T>(static_cast<int64_t>(x));
            }

            return std::trunc(x);
        }

        constexpr uint32_t float_fraction_mask = 0x007fffff;
        constexpr uint32_t float_sign_fraction_mask = 0x807fffff;
        constexpr uint64_t double_fraction_mask = 0x000fffffffffffffULL;
        constexpr uint64_t double_sign_fraction_mask = 0x800fffffffffffffULL;
        constexpr std::size_t sin_lut_size = 1024;
        constexpr std::size_t sin_lut_mask = sin_lut_size - 1;

        constexpr float mul_add(float a, float b, float c)
        {
            if (std::is_constant_evaluated())
            {
                return a * b + c;
            }

#if defined(FP_FAST_FMAF)
            return std::fma(a, b, c);
#else
            return a * b + c;
#endif
        }

        constexpr double mul_add(double a, double b, double c)
        {
            if (std::is_constant_evaluated())
            {
                return a * b + c;
            }

#if defined(FP_FAST_FMA)
            return std::fma(a, b, c);
#else
            return a * b + c;
#endif
        }

        constexpr float sin_poly(float angle)
        {
            angle = static_cast<float>(angle - trunc(angle * static_cast<float>(INV_TAU)) * static_cast<float>(TAU));

            if (angle < 0.0f)
            {
                angle += static_cast<float>(TAU);
            }

            const bool negative = angle > static_cast<float>(PI);

            if (angle > static_cast<float>(PI))
            {
                angle -= static_cast<float>(PI);
            }

            if (angle > static_cast<float>(PI / 2))
            {
                angle = static_cast<float>(PI) - angle;
            }

            float result = 0.0f;

            if (angle < static_cast<float>(PI / 4))
            {
                const float x2 = angle * angle;
                float p = -1.0f / 5040.0f;
                p = mul_add(p, x2, 1.0f / 120.0f);
                p = mul_add(p, x2, -1.0f / 6.0f);
                result = angle * mul_add(x2, p, 1.0f);
            }
            else
            {
                angle -= static_cast<float>(PI / 2);
                const float x2 = angle * angle;
                float p = -1.0f / 720.0f;
                p = mul_add(p, x2, 1.0f / 24.0f);
                p = mul_add(p, x2, -1.0f / 2.0f);
                result = mul_add(x2, p, 1.0f);
            }

            return negative ? -result : result;
        }

        constexpr std::array<float, sin_lut_size> make_sin_lut()
        {
            std::array<float, sin_lut_size> table{};

            for (std::size_t i = 0; i < sin_lut_size; ++i)
            {
                table[i] = sin_poly(static_cast<float>(static_cast<double>(i) * TAU /
                                                       static_cast<double>(sin_lut_size)));
            }

            return table;
        }

        inline constexpr auto sin_lut = make_sin_lut();

        inline float sin_lut_quadratic(float angle)
        {
            constexpr float phase_scale =
                static_cast<float>(static_cast<double>(sin_lut_size) / TAU);

            const float phase = angle * phase_scale;
            int index = static_cast<int>(phase);
            index -= phase < static_cast<float>(index) ? 1 : 0;
            const float t = phase - static_cast<float>(index);

            const std::size_t center = static_cast<std::size_t>(index) & sin_lut_mask;
            const float previous = sin_lut[(center - 1) & sin_lut_mask];
            const float current = sin_lut[center];
            const float next = sin_lut[(center + 1) & sin_lut_mask];

            const float slope = 0.5f * (next - previous);
            const float curvature = 0.5f * (next - 2.0f * current + previous);
            return mul_add(curvature, t * t, mul_add(slope, t, current));
        }

        //
        // double precision sin/cos: L1-resident LUT + small-angle correction
        //
        // Reduce x to a table index over [0, 2pi) plus a tiny residual delta,
        // then sin(x) = sinTab*cos(delta) + cosTab*sin(delta). With
        // trig_lut_size == 512, delta < 2pi/512 ~= 0.0123, so
        //   cos(delta) ~= 1 - delta^2/2   (error < 1e-9)
        //   sin(delta) ~= delta(1 - delta^2/6)
        // already clears the 24-bit (6e-8) target with room to spare.
        // Table: 512 * 2 * 8 B = 8 KB, well within L1; the hot path is
        // branchless (one floor, one mask, one lookup, a few FMAs).
        //
        constexpr std::size_t trig_lut_size = 512;
        constexpr std::size_t trig_lut_mask = trig_lut_size - 1;

        // Self-contained accurate sin used at compile time and to fill the LUT.
        constexpr double sin_double_accurate(double angle)
        {
            angle -= trunc(angle * INV_TAU) * TAU;
            if (angle < 0.0)
            {
                angle += TAU;
            }

            const bool negative = angle > PI;
            if (angle > PI)
            {
                angle -= PI;
            }
            if (angle > PI / 2)
            {
                angle = PI - angle;
            }

            double result = 0.0;
            if (angle < PI / 4)
            {
                const double x2 = angle * angle;
                double p = -1.0 / 39916800.0; // -1/11!
                p = mul_add(p, x2, 1.0 / 362880.0);
                p = mul_add(p, x2, -1.0 / 5040.0);
                p = mul_add(p, x2, 1.0 / 120.0);
                p = mul_add(p, x2, -1.0 / 6.0);
                result = angle * mul_add(x2, p, 1.0);
            }
            else
            {
                angle -= PI / 2;
                const double x2 = angle * angle;
                double p = -1.0 / 3628800.0; // -1/10!
                p = mul_add(p, x2, 1.0 / 40320.0);
                p = mul_add(p, x2, -1.0 / 720.0);
                p = mul_add(p, x2, 1.0 / 24.0);
                p = mul_add(p, x2, -1.0 / 2.0);
                result = mul_add(x2, p, 1.0);
            }
            return negative ? -result : result;
        }

        struct trig_lut_t
        {
            std::array<double, trig_lut_size> sin{};
            std::array<double, trig_lut_size> cos{};
        };

        constexpr trig_lut_t make_trig_lut()
        {
            trig_lut_t table{};
            for (std::size_t i = 0; i < trig_lut_size; ++i)
            {
                const double angle =
                    static_cast<double>(i) * TAU / static_cast<double>(trig_lut_size);
                table.sin[i] = sin_double_accurate(angle);
                table.cos[i] = sin_double_accurate(angle + PI / 2);
            }
            return table;
        }

        inline constexpr trig_lut_t trig_lut = make_trig_lut();

        struct sincos_d
        {
            double sin_a;
            double cos_a;
            double sin_d;
            double cos_d;
        };

        inline sincos_d trig_lut_reduce(double angle)
        {
            constexpr double scale = static_cast<double>(trig_lut_size) / TAU;
            constexpr double inv_scale = TAU / static_cast<double>(trig_lut_size);

            const double phase = angle * scale;
            int64_t k = static_cast<int64_t>(phase);
            k -= (phase < static_cast<double>(k)) ? 1 : 0; // floor
            const double frac = phase - static_cast<double>(k);
            const std::size_t index = static_cast<std::size_t>(k) & trig_lut_mask;

            const double delta = frac * inv_scale;
            const double d2 = delta * delta;

            sincos_d r;
            r.sin_a = trig_lut.sin[index];
            r.cos_a = trig_lut.cos[index];
            r.sin_d = delta * mul_add(d2, -1.0 / 6.0, 1.0);
            r.cos_d = mul_add(d2, -0.5, 1.0);
            return r;
        }

        inline double sin_lut_double(double angle)
        {
            const sincos_d r = trig_lut_reduce(angle);
            // sin(a + d) = sin a cos d + cos a sin d
            return mul_add(r.sin_a, r.cos_d, r.cos_a * r.sin_d);
        }

        inline double cos_lut_double(double angle)
        {
            const sincos_d r = trig_lut_reduce(angle);
            // cos(a + d) = cos a cos d - sin a sin d
            return mul_add(r.cos_a, r.cos_d, -(r.sin_a * r.sin_d));
        }

        //
        // float pow building blocks: pow(b,e) = 2^(e * log2(b)).
        //
        // Both halves use an L1 LUT so the hot path has no division and a short
        // dependency chain. Targets 16-bit relative accuracy; the error in
        // log2(b) is amplified by ~ln2*e in the result, so the table is sized
        // to keep d(log2 b) well under 1e-5.
        //

        // log2(mantissa) table over [1,2): value at 1 + j/L, linearly interpolated.
        constexpr std::size_t pow_log2_lut_size = 256; // L; index = top 8 fraction bits
        constexpr std::size_t pow_exp2_lut_size = 256; // N; 2^(j/N)

        // Self-contained constexpr log2 for table fill: log2(y) = 2*atanh(z)/ln2,
        // z = (y-1)/(y+1). Converges fast for y in [1,2].
        constexpr double log2_for_table(double y)
        {
            const double z = (y - 1.0) / (y + 1.0);
            const double z2 = z * z;
            double term = z;
            double sum = z;
            for (int k = 3; k < 40; k += 2)
            {
                term *= z2;
                sum += term / static_cast<double>(k);
            }
            return 2.0 * sum / LN2;
        }

        constexpr std::array<float, pow_log2_lut_size + 1> make_pow_log2_lut()
        {
            std::array<float, pow_log2_lut_size + 1> table{};
            for (std::size_t j = 0; j <= pow_log2_lut_size; ++j)
            {
                const double y =
                    1.0 + static_cast<double>(j) / static_cast<double>(pow_log2_lut_size);
                table[j] = static_cast<float>(log2_for_table(y));
            }
            return table;
        }

        constexpr std::array<float, pow_exp2_lut_size> make_pow_exp2_lut()
        {
            std::array<float, pow_exp2_lut_size> table{};
            for (std::size_t j = 0; j < pow_exp2_lut_size; ++j)
            {
                // 2^(j/N) = exp((j/N) ln2), Taylor (compile time only).
                const double g =
                    static_cast<double>(j) / static_cast<double>(pow_exp2_lut_size) * LN2;
                double term = 1.0;
                double s = 1.0;
                for (int k = 1; k < 18; ++k)
                {
                    term *= g / static_cast<double>(k);
                    s += term;
                }
                table[j] = static_cast<float>(s);
            }
            return table;
        }

        inline constexpr auto pow_log2_lut = make_pow_log2_lut();
        inline constexpr auto pow_exp2_lut = make_pow_exp2_lut();

        inline float pow_log2_float(float x)
        {
            const uint32_t bits = std::bit_cast<uint32_t>(x);
            const int e = static_cast<int>((bits >> 23) & 0xff) - 127;
            const uint32_t frac = bits & float_fraction_mask;

            const std::size_t idx = frac >> 15;           // top 8 bits -> [0, 256)
            const float t = static_cast<float>(frac & 0x7fff) * (1.0f / 32768.0f);
            const float log2m =
                mul_add(t, pow_log2_lut[idx + 1] - pow_log2_lut[idx], pow_log2_lut[idx]);
            return static_cast<float>(e) + log2m;
        }

        inline float pow_exp2_float(float t)
        {
            if (t >= 128.0f)
            {
                return std::numeric_limits<float>::infinity();
            }
            if (t <= -126.0f)
            {
                return 0.0f;
            }

            constexpr float scale = static_cast<float>(pow_exp2_lut_size);
            constexpr double step = LN2 / static_cast<double>(pow_exp2_lut_size);
            constexpr float k1 = static_cast<float>(step);
            constexpr float k2 = static_cast<float>(0.5 * step * step);

            const float scaled = t * scale;
            int m = static_cast<int>(scaled);
            m -= (scaled < static_cast<float>(m)) ? 1 : 0; // floor
            const float u = scaled - static_cast<float>(m);

            const std::size_t j = static_cast<std::size_t>(m) & (pow_exp2_lut_size - 1);
            const int integer_exp = m >> 8; // log2(N) == 8

            // 2^(u/N): quadratic residual keeps the relative error near float
            // epsilon, which matters because pow amplifies it by ~ln2*exponent.
            const float base = pow_exp2_lut[j];
            const float corr = mul_add(mul_add(k2, u, k1), u, 1.0f);
            const float mantissa = base * corr;

            uint32_t bits = std::bit_cast<uint32_t>(mantissa);
            const int exponent = static_cast<int>((bits >> 23) & 0xff) + integer_exp;
            if (exponent <= 0)
            {
                return 0.0f;
            }
            if (exponent >= 255)
            {
                return std::numeric_limits<float>::infinity();
            }
            bits = (bits & float_sign_fraction_mask) | (static_cast<uint32_t>(exponent) << 23);
            return std::bit_cast<float>(bits);
        }
    }

    //
    // mod
    //
    template <typename T>
    constexpr T pimod(T x)
    {
        T q = detail::trunc(x * static_cast<T>(INV_TAU));
        T r = x - q * static_cast<T>(TAU);

        if (r < 0)
        {
            r += static_cast<T>(TAU);
        }
        return r;
    }

    template <typename T>
    constexpr T taumod(T x)
    {
        T q = detail::trunc(x * static_cast<T>(INV_PI));
        T r = x - q * static_cast<T>(PI);

        if (r < 0)
        {
            r += static_cast<T>(PI);
        }
        return r;
    }

    //
    // log
    ///
    constexpr float log2(float x)
    {
        // Mobius transform (diameter 1/3) + odd series; faster than std here and
        // more accurate than a same-speed mantissa LUT, so kept over a LUT.
        uint32_t bits = std::bit_cast<uint32_t>(x);

        int e =
            static_cast<int>((bits >> 23) & 0xff) - 127;

        uint32_t frac =
            bits & detail::float_fraction_mask;

        float m =
            1.0f + frac * 0x1p-23f;

        float z =
            (m - 1.0f) /
            (m + 1.0f);

        float z2 = z * z;

        float p =
            1.0f / 9.0f;

        p = detail::mul_add(p, z2, 1.0f / 7.0f);
        p = detail::mul_add(p, z2, 1.0f / 5.0f);
        p = detail::mul_add(p, z2, 1.0f / 3.0f);
        p = detail::mul_add(p, z2, 1.0f);

        return static_cast<float>(e) + (2.0f * z * p) * static_cast<float>(LOG2E);
    }

    constexpr double log2(double x)
    {
        uint64_t bits = std::bit_cast<uint64_t>(x);

        int e =
            static_cast<int>((bits >> 52) & 0x7ff) - 1023;

        uint64_t frac =
            bits & detail::double_fraction_mask;

        double m =
            1.0 + frac * 0x1p-52;

        double z =
            (m - 1.0) /
            (m + 1.0);

        double z2 = z * z;

        double p =
            1.0 / 15.0;

        p = detail::mul_add(p, z2, 1.0 / 13.0);
        p = detail::mul_add(p, z2, 1.0 / 11.0);
        p = detail::mul_add(p, z2, 1.0 / 9.0);
        p = detail::mul_add(p, z2, 1.0 / 7.0);
        p = detail::mul_add(p, z2, 1.0 / 5.0);
        p = detail::mul_add(p, z2, 1.0 / 3.0);
        p = detail::mul_add(p, z2, 1.0);

        return static_cast<double>(e) + (2.0 * z * p) * LOG2E;
    }

    constexpr float log(float x)
    {
        // ln(x) = log2(x) / log2(e) = log2(x) * ln2 (division folded to a multiply).
        return log2(x) * static_cast<float>(LN2);
    }

    constexpr double log(double x)
    {
        // ln(x) = log2(x) / log2(e) = log2(x) * ln2 (division folded to a multiply).
        return log2(x) * LN2;
    }

    constexpr float exp(float x);
    constexpr double exp(double x);

    namespace detail
    {
        template <typename T>
        constexpr T pow_from_log_base(T log_base, T exponent)
        {
            return exp(exponent * log_base);
        }
    }

    template <typename T>
    constexpr T log(T base, T x)
    {
        return log2(x) / log2(base);
    }

    //
    // pow
    //
    template <typename T, typename U>
    constexpr auto pow(T base, U exponent)
    {
        using R = std::common_type_t<T, U>;
        const R log_base = log(static_cast<R>(base));
        return detail::pow_from_log_base(log_base, static_cast<R>(exponent));
    }

    // Fused float path: pow(b, e) = 2^(e * log2(b)), both halves LUT-based.
    constexpr float pow(float base, float exponent)
    {
        if (std::is_constant_evaluated())
        {
            return exp(exponent * log(base));
        }

        return detail::pow_exp2_float(exponent * detail::pow_log2_float(base));
    }

    //
    // exp
    //
    constexpr float exp(float x)
    {
        const float ln2 =
            static_cast<float>(LN2);

        const float inv_ln2 =
            static_cast<float>(INV_LN2);

        if (x > 88.0f)
            return std::numeric_limits<float>::infinity();

        if (x < -88.0f)
            return 0.0f;

        int n =
            static_cast<int>(
                x * inv_ln2 +
                (x >= 0.0f ? 0.5f : -0.5f));

        float r =
            x - static_cast<float>(n) * ln2;

        // exp(r)
        // 1+r(1+r(1/2+r(1/6+r(...))))

        float p =
            static_cast<float>(1.0 / 720.0);

        p = detail::mul_add(p, r, static_cast<float>(1.0 / 120.0));
        p = detail::mul_add(p, r, static_cast<float>(1.0 / 24.0));
        p = detail::mul_add(p, r, static_cast<float>(1.0 / 6.0));
        p = detail::mul_add(p, r, static_cast<float>(0.5));
        p = detail::mul_add(p, r, static_cast<float>(1.0));
        p = detail::mul_add(p, r, static_cast<float>(1.0));

        uint32_t bits = std::bit_cast<uint32_t>(p);

        int exponent =
            static_cast<int>((bits >> 23) & 0xff);

        exponent += n;

        if (exponent <= 0)
            return 0.0f;

        if (exponent >= 255)
            return std::numeric_limits<float>::infinity();

        bits &= detail::float_sign_fraction_mask;
        bits |=
            static_cast<uint32_t>(exponent) << 23;

        return std::bit_cast<float>(bits);
    }
    constexpr double exp(double x)
    {
        if (x > 709.0)
            return std::numeric_limits<double>::infinity();

        if (x < -709.0)
            return 0.0;

        int n =
            static_cast<int>(
                x * INV_LN2 +
                (x >= 0.0 ? 0.5 : -0.5));

        double r =
            x - static_cast<double>(n) * LN2;

        // exp(r)

        double p =
            1.0 / 5040.0;

        p = detail::mul_add(p, r, 1.0 / 720.0);
        p = detail::mul_add(p, r, 1.0 / 120.0);
        p = detail::mul_add(p, r, 1.0 / 24.0);
        p = detail::mul_add(p, r, 1.0 / 6.0);
        p = detail::mul_add(p, r, 0.5);
        p = detail::mul_add(p, r, 1.0);
        p = detail::mul_add(p, r, 1.0);

        uint64_t bits = std::bit_cast<uint64_t>(p);

        int exponent =
            static_cast<int>((bits >> 52) & 0x7ff);

        exponent += n;

        if (exponent <= 0)
            return 0.0;

        if (exponent >= 2047)
            return std::numeric_limits<double>::infinity();

        bits &= detail::double_sign_fraction_mask;
        bits |=
            static_cast<uint64_t>(exponent) << 52;

        return std::bit_cast<double>(bits);
    }

    //
    // sin, cos
    //
    constexpr float sin(float angle)
    {
        if (std::is_constant_evaluated())
        {
            return detail::sin_poly(angle);
        }

        return detail::sin_lut_quadratic(angle);
    }

    constexpr double sin(double angle)
    {
        if (std::is_constant_evaluated())
        {
            return detail::sin_double_accurate(angle);
        }

        return detail::sin_lut_double(angle);
    }

    template <typename T>
    constexpr T cos(T angle)
    {
        return sin(angle + static_cast<T>(PI / 2));
    }

    constexpr double cos(double angle)
    {
        if (std::is_constant_evaluated())
        {
            return detail::sin_double_accurate(angle + PI / 2);
        }

        return detail::cos_lut_double(angle);
    }

}
