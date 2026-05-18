# QuantRDOQ/DepQuant Cache Efficiency Experiment

## Summary

Hypothesis: per-size sub-arrays for the RDOQ temporary arrays (m_pdCostCoeff, m_pdCostSig, m_rateIncUp, m_deltaU, m_fullCoeff, m_trellis, etc.) improve L1 cache hit rate by reducing the working set from 4096 elements to the actual TU size (as little as 16 for 4×4 TUs).

**Verdict:** Approach A (per-size sub-arrays with pointer dispatch) adopted. 2.36× faster per-iteration at 16×16 in microbenchmark, 3.1% wall-clock improvement at `--preset medium` 1080p. Bit-exact.

## Hypotheses Tried

### Baseline — Monolithic `[MAX_TB_SIZEY * MAX_TB_SIZEY]` arrays
All temporary arrays allocated to 64×64 = 4096 elements regardless of actual TU size. ~752 KB total per encoder instance. L1 miss rate ~1.5%.

### Approach A — Per-size sub-arrays (ADOPTED)
Five separate array blocks (4×4, 8×8, 16×16, 32×32, 64×64) packed into a single `SizedBuf<T>` allocation (5456 elements total). Pointer dispatch at hot-loop entry selects the correct sub-array via `SizeClass::idx(dim)`. L1 miss rate drops to 0.36%, IPC rises from 2.14 to 3.36. ~1002 KB total allocation but only the active class is touched.

### Approach B — Scan-loop early exit
Arrays remain 4096 elements, but the scan loop caps at `width × height`. No cache benefit — arrays still pull all 4096 lines into cache. Per-iteration ns is identical to baseline.

### Approach C — Template-based size dispatch
Template `xRateDistOptQuant<log2W, log2H>` with compile-time sized arrays. Best raw per-iteration performance but requires templatizing the full calling chain. Too invasive for current codebase.

## Key Results

| Metric | Baseline | Sub-array | Δ |
|--------|----------|-----------|-----|
| Wall (medium, 1080p, 10fr) | 56.77s | 55.01s | **-3.1%** |
| LLC-load-misses | 20.26M | 14.06M | **-30.6%** |
| IPC (cpu_core) | 2.67 | 2.74 | +2.6% |
| L1 miss rate | 1.51% | 1.53% | +0.02pp (noise) |
| Microbench ns/iter (16×16) | 11.9 | 5.0 | **2.36× faster** |

## Files

| Path | Role |
|------|------|
| `src/access_pattern.h` | Shared TUParams, AccessContext interface |
| `src/microbench.cpp` | Main driver: arg parsing, perf stat orchestration |
| `src/mode_baseline.cpp` | 4096 arrays, full scan (baseline) |
| `src/mode_subarray.cpp` | Per-size sub-arrays (Approach A) |
| `src/mode_earlyexit.cpp` | Loop cap at w*h (Approach B) |
| `src/mode_template.h/.cpp` | Template<log2W,log2H> (Approach C) |
| `build.sh` | Full build + perf benchmark orchestration |
| `phase2_perf.sh` | Perf stat phase (hybrid CPU socket fix) |
| `analyze.py` | Combined timing + perf stat analysis |
| `results/` | Raw CSVs + perf stat output for all 40 mode×TU combos |
| `specs/` | Technical specifications (architecture diagrams, data flow) |

## Source Changes

| File | Change |
|------|--------|
| `source/Lib/CommonLib/QuantRDOQ.h` | Added `SizedBuf<T>` template, `SizeClass` helper, replaced 9 monolithic arrays |
| `source/Lib/CommonLib/QuantRDOQ.cpp` | Updated xRateDistOptQuant, rateDistOptQuantTS, forwardRDPCM |
| `source/Lib/CommonLib/QuantRDOQ2.h` | Replaced `m_tplBuf[4096]` with `SizedBuf<CtxTpl>` |
| `source/Lib/CommonLib/QuantRDOQ2.cpp` | Updated xRateDistOptQuantFast |
| `source/Lib/CommonLib/DepQuant.h` | Replaced `m_trellis[4096][2]` with `SizedBuf2<Decisions,2>` |
| `source/Lib/CommonLib/DepQuant.cpp` | Updated constructor and xQuantDQ |

## Related Issues

- GitHub Issue #7: QuantRDOQ/DepQuant array sizing for cache efficiency
- GitHub Issue #8: SAD32 AVX2 inline assembly crash in debug builds (pre-existing, discovered during testing)
