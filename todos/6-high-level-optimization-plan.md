# high-level-optimization-plan

GitHub Issue: https://github.com/opensassi/deepenc/issues/6

## Previous Work

### What Succeeded

- HAD 8x8 AVX2 kernel accepted at 1.07x speedup, wired via DF_HAD8 in asm-sad_avx2.cpp
- Baseline perf profiles collected for fast/slow × 5/50 frame configurations
- Comprehensive TMAM analysis: Retiring 46.6%, Frontend Bound 26.2%, Backend Bound 13.7%, Bad Speculation 13.5%

### What Was Tried

- HAD 16x16 via 4×8x8 function calls — 0.69x regression, archived
- Function-level ASM for SAD, HAD, DQ checkAllRdCosts, interp filterHor/Ver — all <10% improvement
- VVenC stalls 53% of cycles vs x265's ~20% — the gap maps directly to retiring rate

### What Remains

- **Phase 1: PGO** — setup cmake profile target, generate profile, rebuild with `-fprofile-use`, validate TMAM. Est. +20%.
- **Phase 2: Devirtualization** — `perf record -e branches:u` to find indirect call sites, fix top candidates (CodingStructure, UnitBuf/AreaBuf accessors). Est. +8%.
- **Phase 3: SoA data layout** — convert hot structures from AoS to SoA (CodingUnit, TransformUnit, MotionInfo). Est. +5%.
- **Phase 4: Prefetching** — add `_mm_prefetch` for reference frame blocks, CABAC contexts, transform coeffs, mode decision candidates. Est. +3%.
- **Phase 5: Threading** — wavefront parallelism, work-stealing queue, double-buffered pipeline stages. Est. +40-50%.

### Key Technical Details

- Baseline TMAM (fast-5fr): Retiring 46.6%, Frontend Bound 26.2%, L1 I-cache misses 927M, Branch mispredict 1.83%, LLC miss rate 19.58%
- All optimizations must be bit-exact — validate with MD5 after each phase
- Phase gates: Frontend Bound < 15%, Bad Speculation < 10%, LLC miss rate improved, CPU utilization > 600%
- PGO scripts to create: `scripts/pgo/gen-profile.sh`, `scripts/pgo/use-profile.sh`
- Devirtualization targets: `CodingStructure.h`, `Buffer.h`, `Primitives.h`
- SoA targets: `EncCu.cpp` (mode decision hot path), `ContextModelling.cpp`
- Compare to x265: 800% vs 400% CPU utilization gap is the north star
