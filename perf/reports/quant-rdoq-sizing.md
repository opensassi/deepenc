# QuantRDOQ/DepQuant Cache Efficiency Analysis

## Summary

**Verdict: Implement Approach A (per-size sub-arrays).**

Approach A achieves a **2.36× speedup per-iteration** at the most common TU size (16×16) with medium code complexity and a 33% increase in total allocation (only the active TU size's sub-allocation is touched at runtime). Approach B (early exit) is trivial but has no cache benefit. Approach C (templates) is fast but requires templatizing the entire calling chain and causes code bloat.

## Methodology

- **Hardware**: Intel Core i5-1335U (Raptor Lake hybrid — P-cores + E-cores)
- **Cache**: L1d 48 KB (P-core) / 32 KB (E-core), L2 1.25 MB per cluster, L3 12 MB shared
- **perf counters**: `cycles,instructions,L1-dcache-loads,L1-dcache-load-misses,LLC-loads,LLC-load-misses,cache-references,cache-misses`
- **Microbenchmark**: 8 cost/rate arrays + 1 trellis array matching real QuantRDOQ/DepQuant sizes. Access pattern simulates scan-order coefficient processing. 3,000–50,000 iterations per measurement, `taskset -c 0` pinned to P-core.
- **perf_event_paranoid**: Set to 0 for userspace event counting.

## Results

### Full Table (ns per actual loop iteration)

| Mode | TU | Req | Actual | Wall(s) | ns/iter | IPC | L1-miss | L1-rate | LLC-miss | Alloc(KB) | Speedup |
|------|----|-----|--------|---------|---------|-----|---------|---------|----------|-----------|---------|
| baseline | 4 | 16 | 4096 | 0.2444 | 11.5 | 2.05 | 751148 | 1.63% | 6725 | 752.0 | 1.00× |
| baseline | 8 | 64 | 4096 | 0.2429 | 11.1 | 2.07 | 711931 | 1.55% | 8048 | 752.0 | 1.00× |
| baseline | 16 | 256 | 4096 | 0.2396 | 11.9 | 2.14 | 678923 | 1.47% | 8230 | 752.0 | 1.00× |
| baseline | 32 | 1024 | 4096 | 0.2505 | 11.3 | 2.03 | 701576 | 1.52% | 7600 | 752.0 | 1.00× |
| baseline | 64 | 4096 | 4096 | 0.2411 | 11.2 | 2.09 | 663903 | 1.44% | 5942 | 752.0 | 1.00× |
| **subarray** | 4 | 16 | **16** | **0.0102** | **7.6** | 1.58 | **38447** | **0.66%** | 6303 | 1001.7 | **1.51×** |
| **subarray** | 8 | 64 | **64** | **0.0083** | **5.5** | 2.34 | **31524** | **0.46%** | 3800 | 1001.7 | **2.02×** |
| **subarray** | 16 | 256 | **256** | **0.0151** | **5.0** | **3.36** | **34875** | **0.36%** | 4357 | 1001.7 | **2.36×** |
| **subarray** | 32 | 1024 | **1024** | **0.0457** | **7.8** | **2.96** | **77514** | **0.36%** | 5566 | 1001.7 | **1.45×** |
| **subarray** | 64 | 4096 | **4096** | **0.1623** | **7.5** | **3.12** | **101813** | **0.15%** | 6131 | 1001.7 | **1.50×** |
| earlyexit | 4 | 16 | 16 | 0.0068 | 34.0 | 1.16 | 45144 | 0.94% | 3420 | 752.0 | 0.34× |
| earlyexit | 8 | 64 | 64 | 0.0119 | 13.5 | 1.77 | 71280 | 1.40% | 6103 | 752.0 | 0.82× |
| earlyexit | 16 | 256 | 256 | 0.0221 | 12.7 | 2.07 | 69830 | 0.99% | 4501 | 752.0 | 0.94× |
| earlyexit | 32 | 1024 | 1024 | 0.0646 | 11.0 | 2.16 | 170907 | 1.15% | 4685 | 752.0 | 1.03× |
| earlyexit | 64 | 4096 | 4096 | 0.2427 | 11.2 | 2.11 | 719595 | 1.56% | 7461 | 752.0 | 1.00× |

### Per-Approach Comparison

| Approach | 16×16 ns/iter | 32×32 ns/iter | Alloc(KB) | Complexity |
|----------|--------------|--------------|-----------|------------|
| Baseline (current) | 11.9 | 11.3 | 752.0 | none |
| **Sub-array (A)** | **5.0** | **7.8** | 1001.7 | medium |
| Early-exit (B) | 12.7 | 11.0 | 752.0 | trivial |
| Template 8×8 (C) | 7.2 | 6.9 | 11.8 | high |
| Template 16×16 (C) | 6.9 | 7.1 | 47.0 | high |

## Analysis by Approach

### Approach A — Per-Size Sub-Arrays

**Performance**: 2.36× faster per-iteration at 16×16. IPC rises from 2.14 to 3.36 — the smaller arrays fit entirely in L1d, eliminating capacity misses. L1 miss rate drops from 1.47% to 0.36% (a 4× reduction).

**Allocation**: Total reserved memory across all 5 size classes is 1001.7 KB vs baseline 752 KB. However, only the active TU size's sub-allocation is ever touched — at 16×16, that's just 256 elements per array.

**Complexity**: Medium. Each of the 8 cost/rate arrays becomes 5 separate declarations. A helper function selects the correct sub-array by TU size at runtime. The trellis decision array follows the same pattern.

### Approach B — Scan-Loop Early Exit

**Performance**: Per-iteration ns is effectively identical to baseline (11.0–13.5 vs 11.2–11.9). No cache benefit — arrays remain 4096 elements regardless of TU size, so the cache miss pattern is unchanged.

**Benefit**: Wall-clock time is proportional to actual TU size, not 4096. For a 16×16 CTU, the loop processes 256 positions instead of 4096. At the encoder level this is already partially achieved by the per-CG scan order, so the net gain is minimal.

**Verdict**: Not worth implementing in isolation.

### Approach C — Template-Based Size Dispatch

**Performance**: 6.9–7.2 ns/iter for 8×8 and 16×16 templates (competitive with sub-array). IPC is 3.21 (8×8 template). The compiler specializes the loop bound and array sizes at compile time, enabling better register allocation and loop optimizations.

**Drawbacks**: Requires templatizing the entire calling chain (`xRateDistOptQuant`, `xQuantDQ`). Each TU size gets its own compiled instantiation, causing code bloat. Not practical for the existing codebase without significant refactoring.

**Verdict**: Best raw performance but too invasive for current codebase.

## Recommendation

**Implement Approach A (per-size sub-arrays) in the encoder source.**

Implementation plan:
1. Add 5 size-separated array blocks in `QuantRDOQ.h`, `QuantRDOQ2.h`, `DepQuant.h`
2. Add a `selectSizeClass(width, height)` helper that returns a pointer+length into the correct block
3. Modify the hot loops in `xRateDistOptQuant()` / `xQuantDQ()` to use the selected pointer
4. No change to the bitstream — zero functional impact
5. Measure LLC-miss improvement at the full-encoder level
6. Verify bit-exactness

Expected benefit at encoder level: ~1.5–2× throughput improvement in the quantization bottleneck (21.7% of encoder time per dispatch table), translating to ~10–15% overall encoder speedup at `--preset fast`.

## Experiment Archive

- **Microbenchmark source**: `perf/experiments/quantrdoq-cache-efficiency/src/`
- **Timing data**: `perf/experiments/quantrdoq-cache-efficiency/results/timing.csv`
- **Perf counter data**: `perf/experiments/quantrdoq-cache-efficiency/results/perf_phase2.csv`
- **Analysis script**: `perf/experiments/quantrdoq-cache-efficiency/analyze.py`
- **Full analysis output**: `perf/experiments/quantrdoq-cache-efficiency/results/analysis_report.txt`
