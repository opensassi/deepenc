# had-avx2-optimization

GitHub Issue: https://github.com/opensassi/deepenc/issues/4

## Previous Work

### What Succeeded

- SAD ASM for all 4 block sizes (8/16/32/64) completed, accepted, wired via `applyRdCostAsmOverrides()` in `asm-sad_avx2.cpp`
- Per-instance `m_afpDistortFunc` override pattern proven and reusable for HAD
- C++ SIMD reference exists at `RdCostX86.h:655-796`

### What Was Tried

- Partial `vvenc_had8x8_sse2` ASM in GAS inline assembly at `perf/experiments/sad-avx2-port_2026-05-12/had-todo/had_microbench.cpp`
- Algorithm up through transpose and sign-extension verified correct
- Horizontal butterfly stage-3 produces incorrect results due to data dependency bug: stage-3 reads stage-2 values already modified by stage-3 itself
- Current output: 8829 vs expected 43376 (~20% — only lo-half contributing)

### What Remains

1. Fix stage-3 butterfly: compute all 8 results from **original** stage-2 values before storing any (use temp variables for final pair results)
2. Complete lo/hi reorganization (g0=[lo0-3,hi0-3], g1=[lo4-7,hi4-7]) before butterfly
3. Write `vvenc_had16x16_avx2` (YMM variant, 2 passes of 8 rows each)
4. Benchmark and wire into encoder dispatch

### Key Technical Details

- C++ reference: `xCalcHAD8x8_SSE` at `source/Lib/CommonLib/x86/RdCostX86.h:655-796`
- C++ reference (16x16): `xCalcHAD16x16_AVX2` at `RdCostX86.h:2002-2169`
- Target ASM: `source/Lib/CommonLib/x86/avx2/asm-sad_avx2.cpp` (add HAD alongside SAD)
- Experiment dir: `perf/experiments/sad-avx2-port_2026-05-12/had-todo/`
- Baseline: 21.85 ns/call (C++ SIMD), target <15 ns
- x265 reference: `external/x265/source/common/pixel-a.asm` (16,583 lines), `x86util.asm` (HADAMARD macros)
- Register allocation: 16 XMM regs, 3-stage vertical butterfly → 8x8 transpose → sign-extend → reorganize → 3-stage horizontal butterfly → combine → tree reduce
