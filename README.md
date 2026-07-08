# DSPmath

A single-header C++20 arithmetic library for music / audio DSP.
音楽・オーディオ DSP 向けの単一ヘッダ C++20 算術ライブラリ。

`sin`, `cos`, `log`, `log2`, `exp`, `pow` — all `constexpr`, faster than `std` at
runtime while staying within a useful precision budget (≈16-bit for `float`,
≈24-bit for `double`).

[English](#english) · [日本語](#日本語)

---

## English

### Overview

DSPmath provides drop-in replacements for the common transcendental functions
used in audio DSP. The design goal is: **faster than `std` at runtime** with
*enough* precision for audio (relative error within ≈2⁻¹⁶ for `float`,
≈2⁻²⁴ for `double`), while remaining usable at compile time.

- **Header-only.** Just `#include "dspmath.hpp"`.
- **C++20**, no dependencies beyond the standard library.
- **`constexpr` everywhere.** Every function can be evaluated at compile time;
  when an argument (e.g. a `pow` base) is a constant, the work folds away.
- **MIT licensed.**

### Functions

| Function | Notes |
|---|---|
| `DSPmath::sin(x)` / `cos(x)` | `float` & `double` |
| `DSPmath::log(x)` / `log2(x)` | natural log / base-2 |
| `DSPmath::log(base, x)` | arbitrary base |
| `DSPmath::exp(x)` | `float` & `double`, LUT-based at runtime |
| `DSPmath::pow(base, exp)` | fused `2^(e·log2(b))` path for `float` & `double` |
| `DSPmath::pimod(x)` / `taumod(x)` | range-reduction helpers |

### How it works

The implementation follows a consistent recipe:

1. **Analytic range reduction first** — shrink the domain that actually needs
   numerical work, and push as much as possible to `constexpr`.
2. **L1-cache-resident lookup tables + interpolation** instead of long Taylor
   series, where it pays off. Table layout is chosen per function (not always
   linear); linear interpolation is upgraded to quadratic when needed.

Concrete techniques:

- **`sin` / `cos` (double):** angle reduced to a 256-entry sin/cos LUT (4 KB,
  fits L1) plus a small-angle correction `sin(a+δ)=sin a·cos δ + cos a·sin δ`.
  The hot path is branchless.
- **`sin` / `cos` (float):** a 256-entry LUT with quadratic interpolation
  (the table is deliberately small — float-side error is dominated by phase
  rounding, so a bigger table buys nothing).
- **`exp` (float & double):** `exp(x) = 2^(x·log₂e)`, evaluated by a shared
  `2^t` core: a 64-entry LUT (512 B) selects the integer/coarse part via a
  branchless floor (bias trick, no data-dependent branch) and a short
  polynomial handles the fractional remainder (radius ≈1/128 of an octave).
- **`log` / `log2`:** reduced to `log2`, mantissa/exponent split via the log
  laws, and a Möbius transform `z = (m−1)/(m+1)` that shrinks the approximation
  interval to ~1/3 before a short odd-power series, trimmed to just enough
  terms for the target precision.
- **`pow` (float & double):** a dedicated fused `2^(exponent · log2(base))`
  path — both halves LUT-based, no division, and no `ln ↔ log2` conversion
  multiplies. When the base is a compile-time constant, this folds away
  entirely.

### Usage

```cpp
#include "dspmath.hpp"

float  a = DSPmath::sin(0.5f);
double b = DSPmath::log2(8.0);          // 3.0
float  c = DSPmath::pow(2.0f, 3.5f);

// constexpr — computed at compile time
constexpr float k = DSPmath::exp(1.0f); // ~2.71828
static_assert(DSPmath::sin(DSPmath::PI / 6.0) > 0.4999);
```

### Building the benchmark / tests

The library itself needs no build step. The accuracy + timing harness lives in
`tests/compare_std.cpp` and compares every function against `std`.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Or directly with a compiler (optimization and SIMD flags matter):

```bash
# MSVC
cl /std:c++20 /O2 /arch:AVX2 /fp:fast /I. tests/compare_std.cpp

# Clang / GCC
clang++ -std=c++20 -O3 -mavx2 -mfma -ffast-math -I. tests/compare_std.cpp -o compare_std
```

The test fails (non-zero exit) if any function exceeds the 16-bit (`float`) /
24-bit (`double`) relative-error budget.

### Performance

Indicative results on one machine (MSVC `/O2 /arch:AVX2 /fp:fast`, x86-64).
`ratio = DSPmath_ns / std_ns`; **lower is faster**, < 1.0 beats `std`. Numbers
are hardware- and compiler-dependent and have run-to-run noise.

| function | ratio (float) | ratio (double) |
|---|---|---|
| `sin` / `cos` | ~0.71 | **~0.60** |
| `log` | ~0.98 | ~0.74 |
| `log2` | ~0.95 | **~0.46** |
| `exp` | ~1.0 (parity) | ~0.83 |
| `pow` | **~0.86** | ~0.62 |
| `pow` (base 2) | ~0.51 | **~0.26** |

`float` `exp`/`log` are roughly at parity: the platform's `std` (AVX2) is
already excellent there.

> **Note on inlining.** In a large translation unit, MSVC's inline budget can
> run out for functions this small, silently disabling both the runtime LUT
> path *and* compile-time constant folding for known bases/exponents. Every
> hot function in `dspmath.hpp` is marked with a `DSPMATH_INLINE`
> (`__forceinline` / `always_inline`) macro to guarantee inlining regardless
> of call-site count — this alone roughly halved `pow`/`pow2` latency in the
> benchmark TU.

### License

MIT. See the header of `dspmath.hpp`.

---

## 日本語

### 概要

DSPmath は、オーディオ DSP でよく使う超越関数の差し替え用ライブラリです。
設計目標は **ランタイムで `std` より速く**、かつオーディオに*十分*な精度
（`float` で相対誤差 ≈2⁻¹⁶、`double` で ≈2⁻²⁴）を保ちつつ、コンパイル時にも
使えること。

- **ヘッダオンリー。** `#include "dspmath.hpp"` だけ。
- **C++20**、標準ライブラリ以外の依存なし。
- **すべて `constexpr`。** 全関数がコンパイル時評価可能で、引数（例えば `pow`
  の底）が定数なら計算は畳み込まれて消えます。
- **MIT ライセンス。**

### 関数一覧

| 関数 | 補足 |
|---|---|
| `DSPmath::sin(x)` / `cos(x)` | `float` / `double` |
| `DSPmath::log(x)` / `log2(x)` | 自然対数 / 底2 |
| `DSPmath::log(base, x)` | 任意の底 |
| `DSPmath::exp(x)` | `float` / `double`、ランタイムは LUT ベース |
| `DSPmath::pow(base, exp)` | `float` / `double` とも `2^(e·log2(b))` の融合経路 |
| `DSPmath::pimod(x)` / `taumod(x)` | 範囲縮小ヘルパ |

### 仕組み

一貫した方針で実装しています。

1. **まず解析的に変域を狭める** — 数値計算が本当に必要な範囲を縮め、できる
   限り `constexpr` に逃がす。
2. 長いテイラー展開ではなく、**L1 キャッシュに乗る LUT ＋補間**を使う（効く
   場合）。テーブルの取り方は関数ごとに選び（必ずしも線形でない）、線形補間で
   精度が足りなければ2次近似に上げる。

具体的な手法:

- **`sin` / `cos`（double）:** 角度を 256 要素の sin/cos LUT（4 KB、L1 内）に
  落とし、微小角補正 `sin(a+δ)=sin a·cos δ + cos a·sin δ` を加える。ホット
  パスは分岐レス。
- **`sin` / `cos`（float）:** 256 要素の LUT ＋ 2 次補間。テーブルは意図的に
  小さめ（float 側の誤差は位相計算の丸めが支配的で、テーブルを大きくしても
  精度は伸びない）。
- **`exp`（float / double 共通）:** `exp(x) = 2^(x·log₂e)` を共有の `2^t` コア
  で評価。64 要素の LUT（512 B）が整数・粗い部分を分岐レスな floor（バイアス
  トリックでデータ依存分岐を排除）で選び、短い多項式が端数（オクターブの
  約 1/128 の半径）を補正する。
- **`log` / `log2`:** `log2` に帰着し、対数法則で仮数部・指数部を分離、
  メビウス変換 `z = (m−1)/(m+1)` で近似区間を約 1/3 に縮めてから、目標精度に
  ちょうど足りる項数まで削った短い奇数次の級数で評価。
- **`pow`（float / double 共通）:** `2^(指数 · log2(底))` の専用融合経路。
  両半分とも LUT に基づき、除算も `ln ↔ log2` の変換乗算もなし。底がコンパ
  イル時定数なら丸ごと畳み込まれて消える。

### 使い方

```cpp
#include "dspmath.hpp"

float  a = DSPmath::sin(0.5f);
double b = DSPmath::log2(8.0);          // 3.0
float  c = DSPmath::pow(2.0f, 3.5f);

// constexpr — コンパイル時に計算される
constexpr float k = DSPmath::exp(1.0f); // ~2.71828
static_assert(DSPmath::sin(DSPmath::PI / 6.0) > 0.4999);
```

### ベンチマーク / テストのビルド

ライブラリ自体はビルド不要です。精度＋速度の計測ハーネスは
`tests/compare_std.cpp` にあり、各関数を `std` と比較します。

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

コンパイラ直接でも可（最適化・SIMD フラグが効きます）:

```bash
# MSVC
cl /std:c++20 /O2 /arch:AVX2 /fp:fast /I. tests/compare_std.cpp

# Clang / GCC
clang++ -std=c++20 -O3 -mavx2 -mfma -ffast-math -I. tests/compare_std.cpp -o compare_std
```

いずれかの関数が 16bit（`float`）/ 24bit（`double`）の相対誤差予算を超えると
テストは失敗（非ゼロ終了）します。

### 性能

ある環境での参考値（MSVC `/O2 /arch:AVX2 /fp:fast`、x86-64）。
`ratio = DSPmath_ns / std_ns` で、**小さいほど速く**、1.0 未満で `std` 超え。
値はハードウェア・コンパイラ依存で、実行ごとのばらつきもあります。

| 関数 | ratio (float) | ratio (double) |
|---|---|---|
| `sin` / `cos` | ~0.71 | **~0.60** |
| `log` | ~0.98 | ~0.74 |
| `log2` | ~0.95 | **~0.46** |
| `exp` | ~1.0（互角） | ~0.83 |
| `pow` | **~0.86** | ~0.62 |
| `pow`（底2） | ~0.51 | **~0.26** |

`float` の `exp`/`log` はほぼ互角です。この環境の `std`（AVX2）が既に非常に
速いためです。

> **インライン化についての注意。** 大きな翻訳単位では、MSVC のインライン予算
> がこの小ささの関数でも尽きることがあり、ランタイムの LUT パスと、既知の底・
> 指数に対するコンパイル時定数畳み込みの**両方**が黙って無効化されます。
> `dspmath.hpp` のホットパス関数はすべて `DSPMATH_INLINE`（`__forceinline` /
> `always_inline`）マクロを付け、呼び出し箇所数によらずインライン化を保証して
> います。これだけでベンチマークの翻訳単位での `pow`/`pow2` のレイテンシが
> ほぼ半減しました。

### ライセンス

MIT。`dspmath.hpp` のヘッダを参照してください。
