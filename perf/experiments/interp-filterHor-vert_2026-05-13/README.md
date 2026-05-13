# Experiment: interp-filterHor-vert

**Date**: 2026-05-13
**Generated from**: opencode session
**Issue**: https://github.com/opensassi/deepenc/issues/5

## Summary

Attempted to implement three `interp.*` dispatch table entries (filterHor, filterVer, and combined 2D filters) in AVX2 NASM assembly. Work focused on `filterHor` (horizontal 8-tap luma) and `filterVer` (vertical 8-tap luma).

## Results

### filterHor (horizontal 8-tap) — bit-exact, below threshold

| Metric | Value |
|--------|-------|
| C++ SIMD ref | 56.69 ms (100K iter) |
| ASM | 60.62 ms (100K iter) |
| Speedup | **0.935x** |
| Bit-exact | YES |
| P-core IPC | 3.62 (ASM) |

The horizontal filter is bit-exact but ~7% slower than the C++ SIMD reference. The primary bottleneck is the algorithmic choice: 8 overlapping loads vs the compiler's 3 loads + 6 vpshufb approach. On this laptop (15-20% significance threshold), 0.935x is below threshold.

### filterVer (vertical 8-tap) — not bit-exact

The vertical filter was implemented following the same strategy but differs from the C++ reference by 1-8 units on random data. Root causes identified but not yet fixed:
1. Row buffer initialization order (N-1 vs N rows pre-loaded)
2. Extra out-of-bounds load on last iteration
3. Off-by-1 rounding in accumulator order

## Files

```
perf/experiments/interp-filterHor-vert_2026-05-13/
├── README.md                    ← this file
├── src/
│   ├── asm-interp-horiz-8tap.asm    ← bit-exact NASM implementation
│   ├── asm-interp-vert-8tap.asm     ← not bit-exact NASM implementation
│   ├── asm-primitives.h             ← extern "C" declarations
│   ├── asm-primitives.cpp           ← registration
│   ├── build.sh                     ← microbenchmark build script
│   └── interp_microbench.cpp        ← microbenchmark harness
├── specs/
│   ├── interp-filterHor-cpp-spec.spec.md  ← C++ technical spec
│   └── artifacts-interp-filterHor/        ← extracted Mermaid diagrams
└── results/
    ├── microbenchmark.txt          ← C++ vs ASM timing (100K iter)
    └── perf-stat.txt               ← perf stat with counters
```

## Build Infrastructure Changes

- NASM `.asm` support added to `source/Lib/vvenc/CMakeLists.txt`
- Functions registered in `source/Lib/CommonLib/x86/asm-primitives.cpp`
- `{vex3}` prefix required for all YMM instructions using ymm0-ymm7

## Next Steps

See GitHub issue #5 for detailed debugging plan for filterVer.
