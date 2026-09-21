# DSPmath

A single-header C++20 math library for audio DSP.

音楽・オーディオ DSP 向けの、ヘッダ1枚で使える C++20 数学ライブラリです。

[English](#english) · [日本語](#日本語)

## English

DSPmath implements `sin`, `cos`, `log`, `log2`, `exp`, and `pow` for audio
processing, using small lookup tables and polynomial approximations to reduce
computation time. It targets roughly 16-bit accuracy for `float` and 24-bit
accuracy for `double`, with the error measure described below.

Include `dspmath.hpp` to use it. The library requires C++20, depends only on the
standard library, and supports `constexpr` evaluation for every public function.

DSPmath is used in [IMFMSynth](https://imfm-synth.com), an 8-operator synth with
FM and wavetable synthesis.

### Usage

```cpp
#include "dspmath.hpp"

float  a = DSPmath::sin(0.5f);
double b = DSPmath::log2(8.0);          // approximately 3.0
float  c = DSPmath::pow(2.0f, 3.5f);

constexpr float k = DSPmath::exp(1.0f); // evaluated at compile time
static_assert(DSPmath::sin(DSPmath::PI / 6.0) > 0.4999);
```

Use `float` or `double` arguments. For `pow`, passing both arguments with the
same type selects the specialized implementation; mixed types use the generic
`exp(exponent * log(base))` implementation.

| Function | Operation |
|---|---|
| `DSPmath::sin(x)` / `cos(x)` | Sine / cosine; angles in radians |
| `DSPmath::log(x)` / `log2(x)` | Natural / base-2 logarithm |
| `DSPmath::log(base, x)` | Logarithm of `x` to the given base |
| `DSPmath::exp(x)` | e raised to `x` |
| `DSPmath::pow(base, exponent)` | `base` raised to `exponent` |
| `DSPmath::pimod(x)` | Wraps modulo 2π |
| `DSPmath::taumod(x)` | Wraps modulo π |

The two wrapping functions retain their existing names: `pimod` uses a period
of 2π, and `taumod` uses π.

### Accuracy and input ranges

The tests compare results with `std` using this error measure:

```text
abs(result - reference) / max(abs(reference), 1)
```

This is absolute error for reference values between −1 and 1, and relative
error otherwise. The limits are `1.5e-5` for `float` and `6.0e-8` for `double`,
roughly 16 and 24 bits respectively. Some checks also impose an absolute-error
limit. These targets apply to the tested inputs, not every representable value.

The implementations assume IEEE 754 `float` and `double`. Logarithms and the
base of `pow` require positive normal values; the base of `log(base, x)` must
also differ from 1. Zero, negative values, subnormals, NaNs, and infinities do
not receive the full special-case handling of the standard math library.
Exponentiation flushes underflow to zero; `exp(double)` returns zero below −709
and infinity above 709. Trigonometric range reduction is intended for bounded,
finite angles, rather than arbitrarily large inputs.

The main comparison covers angles within ±8τ, logarithm inputs from 0.001 to
256, and exponential inputs from −10 to 10. Additional tests check table
boundaries, a wider range of positive normal doubles for logarithms, and
independent combinations of bases in [0.05, 16] and exponents in [−64, 64]
for `pow(double)`. See the [test sources](tests/) for the exact coverage.

### Implementation

DSPmath first reduces the input range using periodicity and logarithm laws,
then approximates the remaining small interval. Tables are generated at compile
time. The runtime implementations are:

| Function | Method |
|---|---|
| `sin` / `cos`, float | Quadratic interpolation over 256 intervals; a 3 KiB table stores the value, slope, and curvature for each interval |
| `sin` / `cos`, double | A 4 KiB sine/cosine table, with small-angle corrections combined through the angle-addition formulas |
| `log2`, float | Exponent/mantissa decomposition, followed by `z = (m−1)/(m+1)` and a short odd polynomial |
| `log2`, double | A 1 KiB table of reciprocals and logarithms for 64 intervals, followed by a fourth-degree residual polynomial; no division |
| `log` | `log2(x) * ln(2)` |
| `exp` | `2^(x * log2(e))`; float uses a 256-entry table and quadratic correction, double a 64-entry table and cubic correction |
| `pow`, matching float or double arguments | `2^(exponent * log2(base))`, with table-based logarithm and exponential calculations |

Constant evaluation uses separate polynomial implementations where needed.
At runtime, the compiler may fold calculations with known arguments, such as
the logarithm of a fixed base. The main runtime functions use compiler-specific
inlining attributes to help preserve these optimizations in larger translation
units. Neither `constexpr` nor the inlining attributes guarantee that every
runtime call will be folded.

The [implementation notes](CLAUDE.md) explain the formulas and design choices.
The [optimization report](tests/optimization-results.md) records the alternatives
tested, their accuracy, and the measurements behind the current implementation.

### Tests and benchmarks

The library itself needs no build step. To build and run its tests:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

`compare_std` checks accuracy and measures execution time against `std`.
`optimization_accuracy` adds boundary and wider-range checks. To also build
`optimization_benchmark`, which separates independent inputs, dependent inputs,
and competing table accesses, configure with `-DDSPMATH_BUILD_BENCHMARKS=ON`.

The default CMake configuration does not enable AVX2 or fast-math. For the
compiler flags and commands used in the measurements below, see the
[reproduction instructions](tests/optimization-results.md#reproduce-on-msvc).

### Performance

The table below compares the current implementation with `std` on an AMD
Ryzen 7 5700X, Windows x64, using MSVC 19.50 with `/O2 /Ob2 /arch:AVX2 /fp:fast`.
Measurements were taken on 2026-09-20 with `compare_std`: one warmup run,
followed by nine runs. Each value is the median of the reported time ratios.

A ratio of 0.5 means DSPmath took half as long as `std`; lower is better.
Results depend on the CPU, compiler, and workload.

| Function | Time relative to `std`, float | Time relative to `std`, double |
|---|---:|---:|
| `sin` | ~0.73 | ~0.58 |
| `cos` | ~0.73 | ~0.58 |
| `log` | ~0.96 | ~0.77 |
| `log2` | ~0.99 | ~0.57 |
| `exp` | ~1.00 | ~0.82 |
| `pow` | ~0.75 | ~0.52 |
| `pow` (base 2) | ~0.44 | ~0.35 |

This benchmark adds each result to one volatile accumulator, so its timings
include the dependent additions and memory accesses. In a separate test with
independent inputs, the latest LUT changes reduced double `log2` time by about
32% and float `sin` time by about 36% compared with the previous DSPmath
implementation. Details are in the [optimization report](tests/optimization-results.md).

For reference, the following Apple Silicon measurements predate the
2026-09-20 LUT changes. They are medians of three `compare_std` runs on an
Apple A18 Pro (arm64, Apple clang 21, `-O3`). The current implementation has
not been remeasured on that platform.

| Function | Time relative to `std`, float | Time relative to `std`, double |
|---|---:|---:|
| `sin` / `cos` | ~0.71 | ~0.53 |
| `log` | ~0.70 | ~0.73 |
| `log2` | ~0.56 | ~0.73 |
| `exp` | ~0.85 | ~0.63 |
| `pow` | ~0.57 | ~0.56 |
| `pow` (base 2) | ~0.87 | ~0.78 |

### License

MIT. The license text is included in [dspmath.hpp](dspmath.hpp).

---

## 日本語

DSPmath は、音声処理で使う `sin`、`cos`、`log`、`log2`、`exp`、`pow` を提供する
数学ライブラリです。小さな参照テーブルと多項式近似を使い、計算時間を抑えています。
精度の目標は `float` で約16ビット、`double` で約24ビットです。誤差の定義と
検証範囲は後述します。

`dspmath.hpp` をインクルードするだけで使えます。C++20 に対応し、標準ライブラリ
以外の依存はありません。公開関数はすべて `constexpr` に対応しています。

8オペレータで FM と Wavetable が使えるシンセ
[IMFMSynth](https://imfm-synth.com) に採用されています。

### 使い方

```cpp
#include "dspmath.hpp"

float  a = DSPmath::sin(0.5f);
double b = DSPmath::log2(8.0);          // 約 3.0
float  c = DSPmath::pow(2.0f, 3.5f);

constexpr float k = DSPmath::exp(1.0f); // コンパイル時に評価
static_assert(DSPmath::sin(DSPmath::PI / 6.0) > 0.4999);
```

引数には `float` または `double` を使ってください。`pow` は両方の引数を同じ型に
すると専用の実装が選ばれます。型が異なる場合は、汎用の
`exp(exponent * log(base))` による実装を使います。

| 関数 | 計算内容 |
|---|---|
| `DSPmath::sin(x)` / `cos(x)` | 正弦・余弦。角度はラジアン |
| `DSPmath::log(x)` / `log2(x)` | 自然対数・底2の対数 |
| `DSPmath::log(base, x)` | `base` を底とする `x` の対数 |
| `DSPmath::exp(x)` | e の `x` 乗 |
| `DSPmath::pow(base, exponent)` | `base` の `exponent` 乗 |
| `DSPmath::pimod(x)` | 2π を周期として折り返す |
| `DSPmath::taumod(x)` | π を周期として折り返す |

折り返し関数は既存の名前を維持しており、`pimod` の周期が 2π、`taumod` の周期が
π になっています。

### 精度と入力範囲

テストでは `std` の結果を基準に、次の式で誤差を評価します。

```text
abs(計算結果 - 基準値) / max(abs(基準値), 1)
```

基準値が −1 から 1 の間なら絶対誤差、それ以外なら相対誤差になります。
許容値は `float` が `1.5e-5`、`double` が `6.0e-8` で、それぞれ約16ビット、
約24ビットに相当します。一部の検査では、これに加えて絶対誤差の上限も設けています。
この精度目標はテストした入力範囲に対するもので、すべての値で保証するものではありません。

実装は IEEE 754 の `float` と `double` を前提としています。対数の引数と `pow` の
底には正の正規化数を使い、`log(base, x)` の底には 1 以外を指定してください。
ゼロ・負数・非正規化数・NaN・無限大に対して、標準数学ライブラリと同じ処理は
保証していません。指数計算のアンダーフローはゼロに切り捨て、`exp(double)` は
−709 未満でゼロ、709 より大きい値で無限大を返します。三角関数は有限の限られた角度を
扱う設計で、極端に大きな角度には対応していません。

主な比較テストは、三角関数で ±8τ、対数で 0.001〜256、指数関数で −10〜10 を対象に
しています。追加テストでは、テーブルの区間境界、対数のより広い正規化数の範囲、
`pow(double)` の底 [0.05, 16] と指数 [−64, 64] の独立した組合せを検証します。
正確な検証範囲は[テストコード](tests/)を参照してください。

### 実装

周期性や対数法則で入力範囲を狭めてから、残った小さな区間を近似します。
参照テーブルはコンパイル時に生成します。実行時の計算方法は次のとおりです。

| 関数 | 計算方法 |
|---|---|
| `sin` / `cos`、float | 256 区間の二次補間。各区間の値・傾き・曲率を 3 KiB のテーブルに保持 |
| `sin` / `cos`、double | 4 KiB の正弦・余弦テーブルと微小角の近似を、加法定理で組み合わせる |
| `log2`、float | 指数部と仮数部を分離し、`z = (m−1)/(m+1)` に変換して短い奇多項式で評価 |
| `log2`、double | 64 区間の逆数・対数テーブル（1 KiB）と四次の残差多項式で評価。除算は不要 |
| `log` | `log2(x) * ln(2)` |
| `exp` | `2^(x * log2(e))` に変換。float は256要素のテーブルと二次補正、double は64要素のテーブルと三次補正 |
| `pow`、同じ型の float または double 引数 | `2^(exponent * log2(base))` に変換し、対数・指数の両方にテーブルを使用 |

定数評価では、必要に応じて別の多項式実装を使います。実行時の呼出しでも、底の対数
など、引数が既知の部分はコンパイラが事前に計算できる場合があります。主要な関数には
コンパイラごとのインライン指定を付け、大きな翻訳単位でもこうした最適化が働くように
しています。ただし、`constexpr` やインライン指定だけで、すべての呼出しが定数に
畳み込まれるわけではありません。

数式と設計判断は[実装ノート](CLAUDE.md)に、試した手法の精度と採否の根拠は
[最適化レポート](tests/optimization-results.md)にまとめています。

### テストとベンチマーク

ライブラリ自体のビルドは不要です。テストは次の手順で実行できます。

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

`compare_std` は `std` と精度・実行時間を比較し、`optimization_accuracy` は
境界値や、より広い入力範囲を検証します。CMake の設定時に
`-DDSPMATH_BUILD_BENCHMARKS=ON` を指定すると、独立入力・依存入力・別テーブルへの
アクセスを伴う入力を分けて測る `optimization_benchmark` もビルドできます。

CMake の既定設定では AVX2 や fast-math を有効にしていません。以下の計測に使った
コンパイラ設定とコマンドは、[再現手順](tests/optimization-results.md#reproduce-on-msvc)
を参照してください。

### 性能

現行実装と `std` を、AMD Ryzen 7 5700X、Windows x64、MSVC 19.50
（`/O2 /Ob2 /arch:AVX2 /fp:fast`）で比較しました。計測日は 2026-09-20 です。
`compare_std` をウォームアップで1回実行した後、9回実行して、出力された時間比の
中央値を取りました。

0.5 なら `std` の半分の時間で計算できたことを表し、小さいほど高速です。
結果は CPU、コンパイラ、処理内容によって変わります。

| 関数 | `std` に対する時間比、float | `std` に対する時間比、double |
|---|---:|---:|
| `sin` | ~0.73 | ~0.58 |
| `cos` | ~0.73 | ~0.58 |
| `log` | ~0.96 | ~0.77 |
| `log2` | ~0.99 | ~0.57 |
| `exp` | ~1.00 | ~0.82 |
| `pow` | ~0.75 | ~0.52 |
| `pow`（底2） | ~0.44 | ~0.35 |

このベンチマークは各計算結果を1つの volatile 変数に加算するため、前の結果を待つ
加算処理やメモリアクセスの時間も含みます。別途、独立した入力を使った変更前後の
比較では、今回のテーブル最適化により double の `log2` が約32%、float の `sin` が
約36%短縮しました。詳細は[最適化レポート](tests/optimization-results.md)に記載しています。

参考として、2026-09-20 のテーブル変更前に測定した Apple Silicon の結果も掲載します。
Apple A18 Pro（arm64、Apple clang 21、`-O3`）で `compare_std` を3回実行した中央値です。
この環境では現行実装を再計測していません。

| 関数 | `std` に対する時間比、float | `std` に対する時間比、double |
|---|---:|---:|
| `sin` / `cos` | ~0.71 | ~0.53 |
| `log` | ~0.70 | ~0.73 |
| `log2` | ~0.56 | ~0.73 |
| `exp` | ~0.85 | ~0.63 |
| `pow` | ~0.57 | ~0.56 |
| `pow`（底2） | ~0.87 | ~0.78 |

### ライセンス

MIT ライセンスです。条文は [dspmath.hpp](dspmath.hpp) に含まれています。
