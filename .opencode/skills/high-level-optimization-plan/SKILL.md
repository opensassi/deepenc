---
name: high-level-optimization-plan
description: Multi-phase encoder-level optimization — PGO, devirtualization, SoA data layout, prefetching, and threading analysis using perf TMAM methodology (GitHub issue #6)
---

# Skill: high-level-optimization-plan

## Issue Reference

GitHub Issue: https://github.com/opensassi/deepenc/issues/6

## Dependencies

Requires: **profiler** — load this skill first via `skill profiler`.

## Previous Work

### What Succeeded

- HAD 8x8 AVX2 kernel accepted at 1.07x speedup, wired via DF_HAD8 in asm-sad_avx2.cpp
- Baseline perf profiles collected for fast/slow × 5/50 frame configurations
- Comprehensive TMAM analysis revealing retiring rate of only 46.6% with 26.2% frontend bound

### What Was Tried

- HAD 16x16 via 4x8x8 function calls — 0.69x regression, archived
- Function-level ASM for multiple dispatch table entries (SAD, HAD, DQ checkAllRdCosts, interp filterHor/Ver) all yielded <10% improvements

### What Remains

- VVenC stalls 53% of cycles vs x265's ~20% — the gap maps directly to retiring rate
- Frontend bound at 26.2% is the dominant stall (L1 I-cache misses, template bloat, virtual dispatch)
- Backend bound at 13.7% and bad speculation at 13.5% are secondary targets
- Four optimization phases: PGO, devirtualization, SoA/prefetch, threading — targeting 1.4-2.1x cumulative

## Persona

Senior performance engineer with expertise in PGO (LLVM/Clang), C++ devirtualization patterns, SoA data layout design, software prefetching, and parallel threading models for video encoders. You work from perf TMAM data to identify bottlenecks, apply targeted fixes, and validate with bit-exact regression tests.

## On Activation

1. Load the profiler skill: `skill profiler`
2. Check baseline profiles exist in `perf/baseline/profiles/default/`
3. Run `topdown --preset fast --frames 5` and display current TMAM
4. Present the phase roadmap with estimated ROI
5. Ask which phase to begin with, or run `execute` for the full pipeline

## Commands

### `execute [--phases pgo,devirt,soa,prefetch,threads] [--preset fast] [--frames 50]`

Run the full optimization pipeline end-to-end. Each phase validates with perf TMAM and a bit-exact check before proceeding to the next.

```
execute --phases pgo,devirt,soa
```
Runs only phases 1-3, skipping prefetch and threading.

**Phase sequence**:

1. **PGO** — setup cmake profile target, run profile-generate encode, merge profdata, rebuild with `-fprofile-use`, validate TMAM
2. **Devirtualization** — `perf record -e branches:u` to find indirect call sites, fix top candidates
3. **SoA** — profile cache miss heatmap, convert hot structures from AoS to SoA
4. **Prefetch** — add `_mm_prefetch` for known access patterns, validate LLC miss rate
5. **Threading** — analyze thread idle time vs x265 wavefront, propose threading model changes

After each phase, `execute` saves `perf/reports/<phase>-tmam.json` and prints a summary.

### `topdown [--preset PRESET] [--frames N]`

Single TMAM measurement using `perf stat --topdown`. Compares against baseline from `perf/baseline/reports/profile-summary.json`.

Output format:
```
=== TMAM Comparison ===
               Baseline (fast-50fr)   Current               Delta
Retiring        48.8%                  55.1%                +6.3%
Frontend Bound  24.5%                  18.2%                -6.3%
Backend Bound   13.2%                  13.8%                +0.6%
Bad Speculation 13.1%                  10.5%                -2.6%
```

### `phase pgo setup`

Create PGO build infrastructure:

1. Add `CMAKE_BUILD_TYPE=Profile` to root CMakeLists.txt:
   ```cmake
   set(CMAKE_CXX_FLAGS_PROFILE "-fprofile-generate -fprofile-update=atomic")
   set(CMAKE_EXE_LINKER_FLAGS_PROFILE "-fprofile-generate")
   ```

2. Write `scripts/pgo/gen-profile.sh`:
   ```bash
   #!/usr/bin/env bash
   set -euo pipefail
   REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
   BUILD_DIR="$REPO_ROOT/build/pgo-gen"
   cmake -B "$BUILD_DIR" -S "$REPO_ROOT" \
     -DCMAKE_BUILD_TYPE=Profile \
     -DCMAKE_CXX_FLAGS="-fprofile-generate -fprofile-update=atomic" \
     -DCMAKE_EXE_LINKER_FLAGS="-fprofile-generate"
   cmake --build "$BUILD_DIR" -j$(nproc)
   LLVM_PROFILE_FILE="default_%p.profraw" \
     "$BUILD_DIR/bin/release-static/vvencapp" \
     -i "$REPO_ROOT/test/data/park_joy_832x480f50.yuv" \
     -s 832x480 -r 50 -f 5 --preset fast -q 22 -o /dev/null
   llvm-profdata merge -output="$REPO_ROOT/perf/pgo/code.profdata" *.profraw
   ```

3. Write `scripts/pgo/use-profile.sh`:
   ```bash
   cmake -B "$BUILD_DIR/pgo-use" -S "$REPO_ROOT" \
     -DCMAKE_BUILD_TYPE=Release \
     -DCMAKE_CXX_FLAGS="-fprofile-use=$REPO_ROOT/perf/pgo/code.profdata -Wno-profile-instr-unprofiled"
   cmake --build "$BUILD_DIR/pgo-use" -j$(nproc)
   ```

### `phase pgo gen`

Execute `scripts/pgo/gen-profile.sh`. Runs profile-generate encode and merges `.profraw` files.

### `phase pgo use`

Execute `scripts/pgo/use-profile.sh`. Rebuilds encoder with PGO optimization.

### `phase pgo validate`

1. Bit-exact check: encode 5 frames with both baseline and PGO builds, compare output file MD5
2. TMAM comparison: run `topdown --preset fast --frames 50` on PGO build
3. **Gate**: If Frontend Bound > 15% or Retiring < 55%, print warning and suggest checking profile data quality

Reports saved to `perf/reports/pgo-tmam.json`.

### `phase devirt audit`

```
perf record -e branches:u -c 100 -o /tmp/perf-branch.data \
  ./bin/release-static/vvencapp \
  -i test/data/park_joy_832x480f50.yuv \
  -s 832x480 -r 50 -f 5 --preset fast -q 22 -o /dev/null
perf report -i /tmp/perf-branch.data --sort symbol --branch-history | head -50
```

Look for indirect branches (`call *rax`, `call *rdi`, `jmp *rsi`) in hot functions. Top candidates:
- `CodingStructure` virtual method dispatch
- `UnitBuf`/`AreaBuf` accessor chains (inheritance: Area -> Position -> Size)
- Per-module SIMD function pointer tables not yet routed through `g_vvenc`

Save the top-20 indirect call sites to `perf/reports/indirect-calls.txt`.

### `phase devirt <site>`

Given one indirect call site from the audit, propose and implement devirtualization:

```
phase devirt CodingStructure::getCU
```

Strategies (in order of preference):
1. **Template over type**: `template<typename CUType> void processCU(CUType& cu)` — compile-time dispatch
2. **Hot-path bypass**: If 90% of calls go to one type, check and call directly: `if (fast_path) { processFast(cu); } else { virtual_dispatch(cu); }`
3. **Manual vtable**: Store function pointers explicitly instead of virtual dispatch

After fixing, re-run `topdown` to validate Bad Speculation improvement.

### `phase soa audit`

Profile cache behavior to find structures with sparse access:

```bash
perf stat -e LLC-loads,LLC-load-misses,L1-dcache-load-misses \
  ./bin/release-static/vvencapp ...
```

Then identify hot loops that access struct fields sparsely. Key targets:
- `CodingUnit` arrays in `CodingStructure` — mode decision iterates CUs but reads 1-2 fields per CU
- `TransformUnit` — similar sparse access
- `MotionInfo` arrays — scattered reads

For each candidate, compute cache waste: `waste = (sizeof(CodingUnit) * N - hot_field_size * N) / (sizeof(CodingUnit) * N)`.

### `phase soa <struct>`

Convert one struct from AoS to SoA:

```cpp
// Before (AoS):
struct CodingUnit { int mode; int cuType; int qtDepth; int mtDepth; bool skip; ... };
CodingUnit m_cus[MAX_CUS];  // mode-decision loop reads m_cus[i].mode — 3% of each cache line used

// After (SoA):
struct CodingUnitData {
  int* mode;
  int* cuType;
  SignedCodingUnitData signedData; // sub-struct for frequently-accessed fields
};
// mode decision: mode[i] in contiguous array — 100% cache line utilization
```

Steps:
1. Define SoA structure in the relevant header
2. Replace array access with field access: `cu.mode` → `cuData.mode[i]`
3. Update all write sites to use SoA assignment
4. Run `topdown` to validate backend bound improvement

### `phase prefetch <pattern>`

Add `_mm_prefetch` for one access pattern:

| Pattern | Where | Prefetch Strategy |
|---------|-------|-------------------|
| Reference frame blocks | `InterSearch.cpp` ME loops | Prefetch next search MV position while processing current |
| CABAC contexts | `ContextModelling.cpp` | Sequential read-ahead of context array |
| Transform coeffs | `TrQuant.cpp` scan loops | Prefetch next scan group |
| Mode decision candidates | `EncCu.cpp` | Prefetch next partition mode's data |

After adding, validate with:
```bash
perf stat -e LLC-load-misses,LLC-loads ./bin/release-static/vvencapp ...
```

### `phase threads`

Analyze thread utilization:

```bash
perf sched record -F 1000 \
  ./bin/release-static/vvencapp -i test/data/park_joy_832x480f50.yuv ...
perf sched timehist -s > perf/reports/thread-times.txt
```

Measure:
- Thread active % vs sleep/wait %
- Idle time at pipeline stage boundaries
- Load imbalance between threads

Compare against x265 threading model:
- x265: wavefront CTU rows + lookahead + frame encoder threads
- VVenC: `NoMallocThreadPool` with pipeline stages (frame-level)

If threads show >20% idle time:
1. Propose wavefront parallelism: process multiple CTU rows per frame concurrently
2. Implement work-stealing queue for load imbalance
3. Double-buffer pipeline stage inputs

### `phase report`

Generate a final comparison table:

```
=== Optimization Report ===
Date: <date>
Sequence: park_joy_832x480f50, preset=fast, frames=50

| Metric              | Baseline | +PGO  | +Devirt | +SoA   | +Threads | Total Delta |
|---------------------|----------|-------|---------|--------|----------|-------------|
| Retiring            | 46.6%    | ...   | ...     | ...    | ...      | +X%         |
| Frontend Bound      | 26.2%    | ...   | ...     | ...    | ...      | -X%         |
| Backend Bound       | 13.7%    | ...   | ...     | ...    | ...      | -X%         |
| Bad Speculation     | 13.5%    | ...   | ...     | ...    | ...      | -X%         |
| Throughput (fps)    | <baseline>| ...   | ...     | ...    | ...      | 1.XXx       |
```

Reads from `perf/reports/baseline-tmam.json` and all `perf/reports/*-tmam.json` files.

## Baseline TMAM Reference

```
fast-5fr (core counters, from perf/baseline/profiles/default/fast-5fr/perf-comprehensive.txt):
  Retiring:          46.6%
  Frontend Bound:    26.2%
  Backend Bound:     13.7%
  Bad Speculation:   13.5%
  L1 I-cache misses: 927 M (5fr) / 5.3 B (50fr)
  Branch mispredict:  1.83%
  LLC miss rate:     19.58%
  dTLB miss rate:     ~0%
```

## Phase Reference Table

| Phase | Est. Time | Complexity | Est. Gain | Validation Gate |
|-------|-----------|------------|-----------|-----------------|
| PGO | ~1 hr | Low | +20% | Frontend Bound < 15%, Retiring >= 55% |
| Devirtualization | ~2 hrs | Medium | +8% | Bad Speculation < 10% |
| SoA | ~4 hrs | High | +5% | LLC miss rate improved |
| Prefetching | ~1 hr | Medium | +3% | LLC miss rate improved > 1% |
| Threading | ~8 hrs | Very High | +40-50% | CPU utilization > 600% |

## Design Principles

- **Measure first** — Every phase runs `topdown` before and after. No change accepted without TMAM validation.
- **Bit-exact always** — PGO and devirtualization must not change encoder output. Validate with MD5 after each phase.
- **One phase at a time** — Results from earlier phases inform the priority of later ones. Don't optimize what's not a bottleneck.
- **Compare to x265** — The 800% vs 400% CPU utilization gap is the north star. Closing it means making VVenC's utilization competitive.
- **Abort gracefully** — If a phase doesn't meet its validation gate, save the partial results and document why, then move to the next phase.

## Files Reference

| File | Role |
|------|------|
| `CMakeLists.txt` (root) | PGO build type definitions |
| `source/Lib/CommonLib/CodingStructure.h/.cpp` | Devirtualization targets |
| `source/Lib/CommonLib/Buffer.h` | AreaBuf accessor chains |
| `source/Lib/CommonLib/Primitives.h` | g_vvenc dispatch |
| `source/Lib/EncoderLib/EncCu.cpp` | Mode decision hot path |
| `source/Lib/CommonLib/ContextModelling.cpp` | CABAC context prefetch |
| `source/Lib/CommonLib/x86/avx2/asm-sad_avx2.cpp` | SIMD dispatch registration pattern |
| `scripts/pgo/gen-profile.sh` | PGO profile generation (to be created) |
| `scripts/pgo/use-profile.sh` | PGO optimized rebuild (to be created) |
| `perf/baseline/` | Baseline build and profiles |
| `perf/reports/` | Per-phase TMAM reports |
| `.opencode/skills/profiler/SKILL.md` | Profiling infrastructure |
