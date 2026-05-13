# HAD (SATD) AVX2 Optimization — TODO

## Status: Work In Progress

The 8x8 and 16x16 Hadamard (SATD) kernels are the next optimization target for the `dist` module after SAD. The compiler gap is LARGER than SAD due to register spills from C++ array structures.

## Baseline

Mixed-configuration microbenchmark shows:
- IPC: 2.70 (vs SAD's 3.24)
- Backend bound: 50.0% (vs SAD's 35.8%)
- L1-dcache miss rate: 1.16% (vs SAD's 0.02%)

## What's Done

- Baseline benchmark: `had_microbench.cpp` (per-config timing, perf counters)
- Algorithm traced and verified against C++ reference via debug output
- Partial ASM implementation started but produces incorrect results (stage-3 butterfly data dependency bug)

## What Remains

1. Fix the stage-3 butterfly data dependency in `vvenc_had8x8_sse2`
2. Complete the lo→g0 and hi→g1 reorganization before horizontal butterfly
3. Verify bit-exactness against C++ `xCalcHAD8x8_SSE`
4. Extend to `xCalcHAD16x16_AVX2` (YMM version of same algorithm, split into 2×8 rows)
5. Wire into encoder via `applyRdCostAsmOverrides()`

## Files

- `had_microbench.cpp` — microbenchmark + partial ASM implementation
- `build-had.sh` — build script

## Reference

- x265: `pixel-a.asm` (16,583 lines) — SATD for all HEVC block sizes
- VVenC C++: `RdCostX86.h:655-2169` — `xCalcHAD8x8_SSE` + `xCalcHAD16x16_AVX2`
