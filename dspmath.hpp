#pragma once

#include <cmath>
#include <cstdint>
#include <bit>
#include <limits>

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

    union FloatBits
    {
        float f;
        uint32_t i;
    };

    union DoubleBits
    {
        double d;
        uint64_t i;
    };

    //
    // mod
    //
    template <typename T>
    constexpr T pimod(T x)
    {
        T q = std::trunc(x * static_cast<T>(INV_TAU));
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
        T q = std::trunc(x * static_cast<T>(INV_PI));
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
        FloatBits bits{x};

        int e =
            static_cast<int>((bits.i >> 23) & 0xff) - 127;

        uint32_t frac =
            bits.i & 0x7fffff;

        float m =
            1.0f + frac * 0x1p-23f;

        float z =
            (m - 1.0f) /
            (m + 1.0f);

        float z2 = z * z;

        float p =
            1.0f / 9.0f;

        p = p * z2 + 1.0f / 7.0f;
        p = p * z2 + 1.0f / 5.0f;
        p = p * z2 + 1.0f / 3.0f;
        p = p * z2 + 1.0f;

        return static_cast<float>(e) + (2.0f * z * p) * static_cast<float>(LOG2E);
    }

    constexpr double log2(double x)
    {
        DoubleBits bits{x};

        int e =
            static_cast<int>((bits.i >> 52) & 0x7ff) - 1023;

        uint64_t frac =
            bits.i & 0xfffffffffffff;

        double m =
            1.0 + frac * 0x1p-52;

        double z =
            (m - 1.0) /
            (m + 1.0);

        double z2 = z * z;

        double p =
            1.0 / 15.0;

        p = p * z2 + 1.0 / 13.0;
        p = p * z2 + 1.0 / 11.0;
        p = p * z2 + 1.0 / 9.0;
        p = p * z2 + 1.0 / 7.0;
        p = p * z2 + 1.0 / 5.0;
        p = p * z2 + 1.0 / 3.0;
        p = p * z2 + 1.0;

        return static_cast<double>(e) + (2.0 * z * p) * LOG2E;
    }

    constexpr float log(float x)
    {
        // ln(x)
        return log2(x) / static_cast<float>(LOG2E);
    }

    constexpr double log(double x)
    {
        return log2(x) / LOG2E;
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
        return exp<R>(
            static_cast<R>(exponent) *
            log<R>(static_cast<R>(base)));
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

        p = p * r + static_cast<float>(1.0 / 120.0);
        p = p * r + static_cast<float>(1.0 / 24.0);
        p = p * r + static_cast<float>(1.0 / 6.0);
        p = p * r + static_cast<float>(0.5);
        p = p * r + static_cast<float>(1.0);
        p = p * r + static_cast<float>(1.0);

        FloatBits bits{};
        bits.f = p;

        int exponent =
            static_cast<int>((bits.i >> 23) & 0xff);

        exponent += n;

        if (exponent <= 0)
            return 0.0f;

        if (exponent >= 255)
            return std::numeric_limits<float>::infinity();

        bits.i &= 0x807fffff;
        bits.i |=
            static_cast<uint32_t>(exponent) << 23;

        return bits.f;
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

        p = p * r + 1.0 / 720.0;
        p = p * r + 1.0 / 120.0;
        p = p * r + 1.0 / 24.0;
        p = p * r + 1.0 / 6.0;
        p = p * r + 0.5;
        p = p * r + 1.0;
        p = p * r + 1.0;

        DoubleBits bits{};
        bits.d = p;

        int exponent =
            static_cast<int>((bits.i >> 52) & 0x7ff);

        exponent += n;

        if (exponent <= 0)
            return 0.0;

        if (exponent >= 2047)
            return std::numeric_limits<double>::infinity();

        bits.i &= 0x800fffffffffffffULL;
        bits.i |=
            static_cast<uint64_t>(exponent) << 52;

        return bits.d;
    }

    //
    // sin, cos
    //
    constexpr float sin(float angle)
    {
        angle = std::fabs(taumod(angle));

        bool negative = angle > PI;

        angle = pimod(angle);

        if (angle > PI / 2)
            angle = PI - angle;

        float result = 0.0f;

        if (angle < PI / 4)
        {
            float x2 = angle * angle;

            // x - x^3/6 + x^5/120 - x^7/5040
            float p = -1.0f / 5040.0f;
            p = p * x2 + 1.0f / 120.0f;
            p = p * x2 - 1.0f / 6.0f;

            result = angle * (1.0f + x2 * p);
        }
        else
        {
            angle -= PI / 2;

            float x2 = angle * angle;

            // cos
            float p = -1.0f / 720.0f;
            p = p * x2 + 1.0f / 24.0f;
            p = p * x2 - 1.0f / 2.0f;

            result = 1.0f + x2 * p;
        }

        return negative ? -result : result;
    }

    constexpr double sin(double angle)
    {
        angle = std::fabs(taumod(angle));

        bool negative_value = angle > PI;

        angle = pimod(angle);

        if (angle > PI / 2)
        {
            angle = PI - angle;
        }

        double result = 0.0;

        if (angle < PI / 4)
        {
            // sin(x)
            // x * (1 + x^2(-1/6 + x^2(1/120 + x^2(-1/5040 + x^2(1/362880 + x^2(-1/39916800)))))

            double x2 = angle * angle;

            double p =
                -1.0 / 39916800.0; // -1/11!

            p = p * x2 + 1.0 / 362880.0; // 1/9!
            p = p * x2 - 1.0 / 5040.0;   // -1/7!
            p = p * x2 + 1.0 / 120.0;    // 1/5!
            p = p * x2 - 1.0 / 6.0;      // -1/3!

            result =
                angle * (1.0 + x2 * p);
        }
        else
        {
            // cos(x)
            // 1 + x²(-1/2 + x²(1/24 + x²(-1/720 + x²(1/40320))))

            angle -= PI / 2;

            double x2 = angle * angle;

            double p =
                -1.0 / 3628800.0; // -1/10!

            p = p * x2 + 1.0 / 40320.0;
            p = p * x2 - 1.0 / 720.0;
            p = p * x2 + 1.0 / 24.0;
            p = p * x2 - 1.0 / 2.0;

            result = 1.0 + x2 * p;
        }
        return negative_value ? -result : result;
    }

    template <typename T>
    constexpr T cos(T angle)
    {
        return sin<T>(angle + PI / 2);
    }

}
