# Optimization measurements — 2026-09-20

## Decision

Adopted two internal changes, without adding public APIs:

- `log2(double)`: 64 pairs of reciprocal/logarithm values (1 KiB), followed by
  a degree-four `log1p` residual. Runtime division is eliminated; constexpr
  evaluation retains the previous series.
- `sin(float)`: precomputed value/slope/curvature coefficients in a 256-entry
  AoS table (3 KiB), replacing runtime differences of three samples.
  `cos(float)` also benefits through its call to `sin(float)`.

The centered-log alternative and additional public APIs were not adopted.
Rejected implementations were removed after measurement. The tables below
record their results; the maintained benchmark measures the public functions.

## Environment and method

- AMD Ryzen 7 5700X, Windows x64.
- MSVC 19.50.35720.0, Visual Studio 18 2026, C++20.
- Timing: `/O2 /Ob2 /DNDEBUG /arch:AVX2 /fp:fast`.
- Additional accuracy build: `/O2 /Ob2 /DNDEBUG /fp:strict`, without AVX2.
- Baseline header: commit `e364e7c798a8dcdcd4dc718052f3bc4efa52f770`.
- Final harness: `tests/optimization_benchmark.cpp`, 4,096 seeded random inputs
  in [0.05,16], 256 passes per sample, one warmup, nine measured samples.
  Function order alternates; results below are median nanoseconds per call.
- Independent mode uses four accumulators, with one final volatile store.
  Dependent mode feeds `x + previous_result * 0.00001` into the next call;
  its timings include that common scale/add overhead, not just function latency.
- Table-pressure mode additionally reads a synthetic 64 KiB float table.
  This is not an IMFMSynth workload or a claim about end-to-end application speed.
- Runtime pow timing uses varying bases and exponent 1.7. Accuracy separately
  checks independent base/exponent combinations.
- Generated assembly was inspected: adopted tables are referenced in the hot
  loops, FMA instructions are present in the fast build, and the benchmark
  contains no `vdivsd` for the new logarithm path. Loop work was not replaced
  by a compile-time result.

## Integrated public-function measurements

Each cell is **before → after**, in ns/call. Lower is better.

| Function | Independent | Dependent | Table pressure |
|---|---:|---:|---:|
| log2(double) | 2.046 → 1.393 | 15.250 → 11.459 | 2.415 → 1.588 |
| log(double) | 2.027 → 1.479 | 14.581 → 12.103 | 2.421 → 1.649 |
| pow(double) | 5.906 → 5.355 | 25.821 → 22.791 | 6.281 → 5.653 |
| sin(float) | 1.966 → 1.252 | 11.207 → 9.957 | 2.173 → 1.404 |
| cos(float) | 2.118 → 1.374 | 11.842 → 10.701 | 2.303 → 1.560 |

Independent-mode time reductions are approximately 32%, 27%, 9%, 36%, and
35%, respectively. A second run with executable order reversed reproduced
the direction and magnitude: baseline independent medians were
2.040 / 2.028 / 5.897 / 1.964 / 2.122; adopted medians were
1.389 / 1.465 / 5.328 / 1.248 / 1.373 ns/call.
These are measurements on one CPU/compiler, not architecture-independent guarantees.

## Candidate screening

Screening used the same input distribution, four accumulators/dependent input,
one warmup and nine samples, but 128 passes and a separate translation unit.
Do not combine these absolute timings with the integrated table above.

| Candidate | Independent ns | Dependent ns | Decision |
|---|---:|---:|---|
| Baseline log2(double) | 2.030 | 15.250 | Reference |
| Recentered log2, five odd terms | 2.833 | 15.594 | Reject |
| 16 intervals, degree five | 1.598 | 12.515 | Reject in favor of 64 |
| 32 intervals, degree four | 1.365 | 11.444 | Reject in favor of 64 |
| 64 intervals, degree four | 1.358 | 11.443 | Adopt |
| Baseline pow(double) | 5.979 | 25.797 | Reference |
| Pow with recentered log2 | 14.726 | 26.840 | Reject |
| Pow with 16-interval log2 | 5.495 | 23.436 | Reject in favor of 64 |
| Pow with 32-interval log2 | 5.177 | 22.543 | Reject in favor of 64 |
| Pow with 64-interval log2 | 5.178 | 22.538 | Adopt |
| Baseline sin(float) | 1.944 | 11.221 | Reference |
| Precomputed coefficients, AoS | 1.227 | 9.960 | Adopt |
| Precomputed coefficients, SoA | 1.232 | 10.072 | Reject in favor of AoS |
| Separate sin(double) + cos(double) | 2.120 | 12.927 | Reference |
| Shared reduction, sum of sin/cos | 2.116 | 12.280 | No new API |
| Cycle-to-radian conversion + coefficient sin | 1.227 | 9.947 | Reference |
| Cycle phase directly + coefficient sin | 1.227 | 9.943 | No new API |
| Runtime fixed-base pow | 6.557 | 13.440 | Reference |
| Precomputed logarithm of runtime base | 2.427 | 15.336 | No new API |

The 32- and 64-interval logs were essentially tied in speed. The 64-entry
choice has more accuracy headroom for pow: screening maximum scaled log2
errors were 2.52e-10 versus 8.13e-12. Both passed; the extra 512 B is deliberate.
AoS and SoA were essentially tied; AoS keeps each interval's coefficients together.

API prototypes measured the operations inline, without a call-boundary cost.
The sincos benchmark consumes both results as a sum; separate calls already
allow the compiler to eliminate common work. The phase conversion can fold
into the table scale under fast-math. Prepared-base timing varied by dependency
pattern; it is not evidence that caching a logarithm is universally slower or
faster. Without a demonstrated application need, these results do not justify
expanding the public API. No application integration was attempted.

## Accuracy and verification

Both `compare_std` and `optimization_accuracy` passed in fast and strict builds
(four successful CTest cases). Constexpr static assertions passed in both.
The additional test covers 200,001 mantissa samples with exponents spanning
[-1000,1000], both sides of every 64-entry LUT boundary at representative
exponents, minimum/maximum positive normal doubles, and cancellation near 1.
Pow covers 513 bases in [0.05,16] × 257 exponents in [-64,64]. Float trig tests
cover ±8τ, LUT boundaries, quarter-step bias-rounding transitions and adjacent
float values. These are finite-domain checks, not a proof over all bit patterns.

Error is `abs(actual-reference) / max(abs(reference),1)`.

| Function | Fast maximum error | Strict maximum error | Limit |
|---|---:|---:|---:|
| log2(double) | 8.12972e-12 | 8.12972e-12 | 6e-8 |
| log(double) | 5.63505e-12 | 5.63505e-12 | 6e-8 |
| pow(double) | 7.73773e-10 | 7.73786e-10 | 6e-8 |
| sin(float) | 5.76148e-6 | 6.66777e-6 | 1.5e-5 |
| cos(float) | 6.16658e-6 | 7.22072e-6 | 1.5e-5 |

## Reproduce on MSVC

Run from the repository root in PowerShell:

```powershell
cmake -S . -B build/validated-fast -DDSPMATH_BUILD_BENCHMARKS=ON '-DCMAKE_CXX_FLAGS_RELEASE=/O2 /Ob2 /DNDEBUG /arch:AVX2 /fp:fast /FAs'
cmake --build build/validated-fast --config Release
ctest --test-dir build/validated-fast -C Release --output-on-failure
./build/validated-fast/Release/optimization_benchmark.exe

cmake -S . -B build/validated-strict '-DCMAKE_CXX_FLAGS_RELEASE=/O2 /Ob2 /DNDEBUG /fp:strict'
cmake --build build/validated-strict --config Release
ctest --test-dir build/validated-strict -C Release --output-on-failure
```

To measure the previous header with the maintained harness:

```powershell
$baselineText = git show e364e7c798a8dcdcd4dc718052f3bc4efa52f770:dspmath.hpp
$baselineFile = Join-Path (Get-Location) 'build/baseline.hpp'
[IO.File]::WriteAllLines($baselineFile, $baselineText, [Text.UTF8Encoding]::new($false))
cmake -S . -B build/benchmark-before -DDSPMATH_BUILD_BENCHMARKS=ON "-DDSPMATH_BENCHMARK_HEADER=$baselineFile" '-DCMAKE_CXX_FLAGS_RELEASE=/O2 /Ob2 /DNDEBUG /arch:AVX2 /fp:fast'
cmake --build build/benchmark-before --config Release --target optimization_benchmark
./build/benchmark-before/Release/optimization_benchmark.exe
```

The header override affects only `optimization_benchmark`; accuracy targets
always test the current library. Run the two benchmark executables sequentially,
then reverse their order. Do not run competing timing processes in parallel.
