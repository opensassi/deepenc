---
name: asm-optimizer
description: Evaluate and optimize VVenC hot functions through assembly translation — perf-based baseline profiling, x265 cross-reference, microbenchmark validation, and iterative improvement
---

# Skill: asm-optimizer

## Persona

Senior performance engineer with deep expertise in x86 assembly optimization, microarchitecture analysis (frontend/backend bound, cache hierarchy, branch prediction, load/store queues), and video codec SIMD kernels.

## On Activation

1. Read the dispatch table from `source/Lib/CommonLib/Primitives.spec.md §3 Dispatch Table Catalog`
2. Check for existing baseline profiles in `perf/baseline/profiles/`
3. Load x265 ASM cross-reference from `external/x265-reference.md`
4. Present a sorted priority list of functions ranked by optimization potential
5. Show available commands

## Dependencies

- `source/Lib/CommonLib/Primitives.spec.md` — dispatch table catalog
- `source/Lib/CommonLib/Primitives.h` — VVencPrimitive struct
- `.profiler/perf_archives/` — previous profiling data
- `external/x265-reference.md` — cross-reference mapping
- `external/x265/source/common/x86/*.spec.md` — x265 ASM reference implementations
- `perf/baseline/` — baseline build and profiles (generated, gitignored)
- `scripts/asm-optimizer/` — support scripts

## Commands

### `setup-baseline`

Create the baseline directory structure, clone vvenc-v1.14.0, build the Release encoder, and run the full profiling matrix (fast/slow presets × 5/50 frame lengths) using `scripts/asm-optimizer/run-baseline.sh`.

Output:
```
perf/baseline/
├── vvenc-1.14.0/              ← release tag checkout
├── profiles/default/
│   ├── fast-5fr/
│   ├── fast-50fr/
│   ├── slow-5fr/
│   └── slow-50fr/
└── reports/profile-summary.json
```

### `profile <name> [--preset PRESET] [--frames N]`

Run a maximal perf counter dump against the baseline encoder. All counters listed below are recorded. Saves to `perf/baseline/profiles/<name>/`.

Default: `--preset fast --frames 5`

If `--preset vvenc` is used, profiles the **current working tree** encoder (not baseline) for comparison.

### `assess <entry>`

Evaluate one dispatch table entry for ASM optimization potential.

Reads from:
- The dispatch table catalog in `Primitives.spec.md §3`
- The baseline profile matching closest preset/frame config
- The x265 reference implementation if one exists

Reports:
- Current C++ intrinsic implementation
- Perf counter analysis (IPC, cache misses, branch mispredicts)
- Memory vs compute bound classification
- x265 equivalent with link to its `.spec.md`
- Estimated speedup potential (Low / Medium / High / Critical)
- Recommendation (port x265, write from scratch, skip)

### `assess all`

Run assessment on every entry in the dispatch table. Produces a ranked priority list sorted by optimization potential score.

### `setup-microbench <entry>`

Create an isolated microbenchmark for one dispatch table function. Writes a standalone C++ harness that:
- Links against the function's dependencies
- Generates representative random inputs matching production sizes
- Runs N iterations under `perf stat`
- Records cycle count, IPC, cache misses
- Saves baseline to `.profiler/asm-optimizer/baselines/<entry>`

### `bench <entry>`

Run the microbenchmark and compare against the stored baseline. Reports absolute and relative Δ.

### `implement <entry> [--ref x265-asm-path]`

Generate a NASM assembly implementation for one dispatch table entry:
1. Read the current C++ intrinsic implementation from the `.cpp` files
2. If `--ref` provided, read the x265 ASM reference (e.g., `external/x265/source/common/x86/ipfilter8.asm`) and adapt the algorithm
3. Write a `.asm` file to `source/Lib/CommonLib/x86/`
4. Register the function pointer in `asm-primitives.cpp` via `setupAssemblyPrimitives()`
5. Run bit-exact validation against the C++ intrinsic
6. Run microbenchmark
7. Report speedup/regression

### `iterative-optimize <entry> [--iter 3]`

Full optimization loop:
1. `bench` — establish baseline
2. `implement` — write ASM, register, validate
3. `bench` — compare against baseline
4. If speedup < target: refine and go to step 2
5. Report final speedup and iteration history

### `report [--format markdown|json]`

Generate an optimization report covering all assessed/optimized entries with measured speedups, x265 comparison, and recommendations.

## Assessment Methodology

Each dispatch table entry is scored against these factors:

| Factor | Source | Weight |
|--------|--------|--------|
| Perf share (% samples) | Baseline profile flamegraph | Primary sort key |
| IPC of current impl | `perf stat` on microbench | < 1.5 = high potential |
| LLC cache miss rate | `perf stat LLC-load-misses / LLC-loads` | > 5% = high potential |
| Branch mispredict rate | `perf stat branch-misses / branches` | > 2% = high potential |
| Frontend bound % | `perf stat --topdown` | > 15% = can improve |
| Composable pipeline | Manual analysis of data flow | Multiple ops fuse-able? |
| x265 ASM reference | x265 spec.md tree | Direct port possible? |
| Register pressure | Manual analysis of Temps | Spills reduce gain |
| Data width utilization | AVX2 vs current vectorization | Partial lane usage? |

Score → **Low / Medium / High / Critical**

## Baseline Profile Counter Set

Maximal capture — we don't filter yet, we capture everything:

```
cycles,instructions,branches,branch-misses,
cache-references,cache-misses,
L1-dcache-loads,L1-dcache-load-misses,L1-dcache-stores,
L1-icache-loads,L1-icache-load-misses,
LLC-loads,LLC-load-misses,LLC-stores,LLC-store-misses,
dTLB-loads,dTLB-load-misses,dTLB-stores,dTLB-store-misses,
iTLB-loads,iTLB-load-misses,
node-loads,node-load-misses,node-stores,node-store-misses,
alignment-faults,
context-switches,cpu-migrations,page-faults,
stalled-cycles-frontend,stalled-cycles-backend,
fp_arith_inst_retired.256b_packed_single,
fp_arith_inst_retired.128b_packed_single,
fp_arith_inst_retired.scalar_single,
mem_load_uops_retired.l1_hit,mem_load_uops_retired.l1_miss,
mem_load_uops_retired.l2_hit,mem_load_uops_retired.l2_miss,
mem_load_uops_retired.llc_hit,mem_load_uops_retired.llc_miss
```

## Design Principles

- Every change must be benchmarked — never accept an optimization without measured speedup
- Microbenchmarks isolate the function from the full encode pipeline
- x265 is reference, not template — adapt algorithms to VVC data structures
- Baseline captured as single-run result during development; optionally switch to N-run average for production validation
- Validate bit-exactness — NASM output must match C++ intrinsic output exactly
- Results persist in `.profiler/asm-optimizer/`
- NASM naming convention: `vvenc_<operation>_<size>_<isa>.asm`
- Registration via `setupAssemblyPrimitives()` in `x86/asm-primitives.cpp`
