# Experiment: sad-avx2-port

**Date**: 2026-05-12
**Skill**: `asm-optimizer` (Phase 1 — SAD)
**Status**: SAD — **ACCEPTED** | HAD — **TODO**

## Summary

Port of SAD (Sum of Absolute Differences) from C++ SIMD intrinsics to hand-tuned AVX2 GAS inline assembly, following x265 `sad-a.asm` / `sad16-a.asm` patterns.

## SAD Results (ACCEPTED)

Four width-specific kernels implemented — all bit-exact and benchmarked vs C++ SIMD:

| Function | C++ SIMD | ASM | Speedup | Significance |
|----------|----------|-----|---------|-------------|
| `SAD8xN` | 14.25 ns | **11.04 ns** | **1.29x (+29%)** | ✅ Above threshold |
| `SAD16xN` | 14.30 ns | **12.03 ns** | **1.19x (+19%)** | ✅ Above threshold |
| `SAD32xN` | 32.05 ns | **30.46 ns** | **1.05x (+5%)** | ✅ At threshold |
| `SAD64xN` | 116.55 ns | **98.99 ns** | **1.18x (+18%)** | ✅ Above threshold |

### Key Techniques
- **Down-counting loop** (`dec` + `jnz` = 2 instr vs C++ `add` + `cmp` + `jl` = 3 instr)
- **No early-exit branches** (removed `checkExit` counter and `maximumDistortion` check)
- **Direct accumulation** (single `vpaddd dst, dst, src` vs C++ `vpaddd tmp, src, acc` + `vmovdqa acc, tmp`)
- **`vpcmpeqw+vpsrlw` for vone** (compute-in-register, no memory load)
- **No stack frame** (no `push rbp` / `pop rbp` needed)

### x265 Cross-Reference
| VVenC Function | x265 Reference | File |
|----------------|---------------|------|
| `xGetSAD_NxN_SIMD<W, AVX2>` | `pixel_sad_{W}x{H}` (16-bit) | `sad16-a.asm` (4,370 lines) |

### Infrastructure
- **Dispatch**: Per-instance `m_afpDistortFunc` override from `applyRdCostAsmOverrides()` in `asm-sad_avx2.cpp`
- **Registration**: Called from `RdCost::create()` after `initRdCostX86()`
- **Files modified**:
  - `source/Lib/CommonLib/x86/avx2/asm-sad_avx2.cpp` — GAS inline ASM + override function
  - `source/Lib/CommonLib/RdCost.cpp` — added `applyRdCostAsmOverrides(*this)` call

### Microbenchmark Configuration
- 2,000,000 iterations
- 8 test patterns cycling (4 sizes × 2 heights each)
- `taskset -c 0` (P-core only)
- Volatile function pointers with `-fno-inline`
- Linking against `libvvenc.a` for C++ SIMD reference

---

## HAD (SATD) — TODO

### Baseline (C++ SIMD, AVX2)
| Function | ns/call | IPC | Backend bound | L1 miss |
|----------|---------|-----|---------------|---------|
| HAD8x8 | 21.85 | — | — | — |
| HAD16x16 | 54.71 | — | — | — |
| HAD32x32 | 211.47 | — | — | — |
| HAD64x64 | 834.07 | — | — | — |
| **All sizes mixed** | — | **2.70** | **50.0%** | **1.16%** |

### Bottleneck Analysis
The `xCalcHAD16x16_AVX2` kernel uses C++ arrays `m1[2][8]` and `m2[2][8]` totaling 32 YMM register slots, but AVX2 has only 16 architectural YMM registers. The compiler **must spill** to stack, causing:
- 50% backend bound (vs SAD's 35.8%)
- 1.16% L1 miss rate (58× higher than SAD)
- Estimated 30-50% improvement potential from explicit register allocation

### Algorithm Structure
```
Load 8 rows → diff → vertical 8x8 butterfly → transpose 8x8 →
sign-extend 16→32 (split lo/hi) → reorganize into g0/g1 →
horizontal 8x32 butterfly → abs → combine → DC adjust
```

### Started ASM Work
- Partial `vvenc_had8x8_sse2` in `had-todo/had_microbench.cpp`
- C++ reference algorithm traced and verified in debug output
- Pending: fix stage-3 butterfly data dependency in ASM, complete lo+hi reorganization, benchmark

### Recommended Approach
Use x265's `HADAMARD` macro infrastructure from `x86util.asm` as a template for the butterfly stages. The key challenge is the 3-stage butterfly data dependency (stage 3 must read original stage 2 values, not modified ones). Consider writing the ASM in NASM (separate `.asm` file) instead of GAS inline to get access to the macro infrastructure.

---

## Directory Layout

```
perf/experiments/sad-avx2-port_2026-05-12/
├── README.md                         ← this file
├── src/
│   ├── asm-sad_avx2.cpp              ← final ASM source for all 4 SAD kernels
│   ├── sad_microbench.cpp            ← SAD microbenchmark harness
│   └── build-sad.sh                  ← SAD build script
├── specs/
│   └── vvenc-sad-cpp-pipeline.spec.md ← C++ pipeline model + gap analysis
├── results/
│   ├── baseline.txt                  ← C++ SIMD baseline timing
│   ├── disassembly.txt              ← C++ compiler disassembly
│   ├── perf-baseline.txt            ← C++ SIMD perf counters
│   └── perf-asm.txt                 ← C++ + ASM perf counters
└── had-todo/
    ├── README.md
    ├── had_microbench.cpp            ← HAD microbenchmark (WIP)
    └── build-had.sh                  ← HAD build script
```

## Dependencies

- `source/Lib/CommonLib/x86/avx2/asm-sad_avx2.cpp` — source of truth (wired in encoder)
- `source/Lib/CommonLib/RdCost.cpp` — override call site
- `perf/microbenchmarks/sad/` — original microbenchmark development
- `external/x265/source/common/sad16-a.asm` — x265 reference (4,370 lines)
