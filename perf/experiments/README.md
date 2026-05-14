# Experiments Index

> Log of all ASM optimization experiments performed through the `asm-optimizer` skill.
> Each entry links to the experiment directory and records the outcome.

## Experiments

| Date | Directory | Description | Outcome | Agent |
|------|-----------|-------------|---------|-------|
| 2026-05-12 | `fix-dq-checkAllRdCosts-asm-minselect_2026-05-12/` | DQ `checkAllRdCosts` min-select blend mask bug fix and ASM implementation | **Archived** — ASM 0.93x vs C++ SIMD, within noise at encoder level. Memory-bound, not CPU-bound. | `dq-asm-minselect-debug-3` |
| 2026-05-12 | `sad-avx2-port_2026-05-12/` | SAD AVX2 port for all 4 block sizes (8/16/32/64) | **Accepted** — 5-29% speedup in microbench, wired into encoder dispatch. | `asm-optimizer` |
| 2026-05-13 | `had-avx2-optimization_2026-05-13/` | HAD (SATD) 8x8 AVX2 + 16x16 AVX2 (archived) | **Accepted** (8x8, 1.07x), **Archived** (16x16, 0.69x) | `had-avx2-optimization-4` |

## Metrics Legend

| Outcome | Meaning |
|---------|---------|
| **Accepted** | Above significance threshold. ASM merged into working tree. |
| **Archived** | Below significance threshold or regression. Experiment saved for reference. |
| **TODO** | Optimization opportunity identified but not yet attempted. |
| **WIP** | Work in progress — partial implementation exists. |

## Directory Layout

Each experiment directory follows this convention:

```
<function>-<description>_<YYYY-MM-DD>/
├── README.md           — session summary, results, conclusions
├── src/                — ASM source, microbenchmark, build script
├── specs/              — pipeline specs, gap analysis, diagrams
└── results/            — benchmark data, perf stat output, comparison tables
```

## Experiment Details

### `had-avx2-optimization_2026-05-13/`

HAD (SATD) 8x8 and 16x16 AVX2:

| Kernel | Outcome | Speedup | Detail |
|--------|---------|---------|--------|
| 8x8 (XMM) | **Accepted** | 1.07x (+7%) | Correct, bit-exact, wired via DF_HAD8 |
| 16x16 (4×8x8 calls) | **Archived** | 0.69x (-31%) | 4 function calls + per-call stack alloc slower than YMM C++ |

Bugs fixed during development:
- Transpose T.S3 column 5/6 swapped vs C++ reference
- Transpose T.S2 pair ordering (t4,t5 vs t2,t3)
- DC value overwritten by g0_abs[7] spill
- Wrapper accumulator clobbered by kernel's r10d usage
- Row advance off by 2× (single lea vs double lea for Pel vs byte stride)
- GAS inline asm push/pop not assembling on single line
