# VVenC Performance Analysis

Reference document capturing TU pipeline memory footprint, threading model, and optimization analysis.

## TU Processing Pipeline — Memory & Execution Profile

### Per-step memory and computation for one TU × one component

| Step | Buffer | Size | Type | Computation | OoO Character |
|------|--------|------|------|-------------|---------------|
| 1. Prediction | `cs.m_pred` sub-buf | 8 KB | `Pel`×4096 | `predIntraAng()` or `motionCompensation()` — gather reference samples, angle interpolation | Load-heavy: scattered reads from reference frame (L2→L3). Bottleneck: memory latency. |
| 2. Residual | `cs.m_resi` sub-buf | 8 KB | `Pel`×4096 | `resi[i] = org[i] - pred[i]` element-wise subtract | Store stream: 1 load + 1 store per element. Saturates store buffer. Streams through L1. |
| 3. Fwd transform | `m_blk` | 16 KB | `TCoeff`×4096 | Copy residual to TCoeff format | Load+store linear copy. Bandwidth-bound. |
| | `m_tmp` | 16 KB | `TCoeff`×4096 | 1D horizontal DCT/DST butterfly | Compute-heavy: FMAs, shuffles, adds. ~4-8 IPC on SIMD. Bottleneck: port 0/1 (FMA). |
| | `m_plTempCoeff` | 16 KB | `TCoeff`×4096 | 1D vertical DCT/DST + transpose | Same as above. Full cache line writes. |
| | Stack temps | 500 B | `int` | E/O/EE/EO butterfly sub-arrays | Register-spill tier: stays in L1. |
| 4. LFNST (opt) | `m_tempInMatrix[48]` | 192 B | `TCoeff` | Gather 48 coeffs into sparse matrix | Scatter-gather: irregular index pattern. Bottleneck: address gen. |
| | `m_tempOutMatrix[48]` | 192 B | `TCoeff` | 48×48 matrix multiply | Compute-light. |
| 5. Quant + RDOQ | `m_plTempCoeff` (reused) | 16 KB | `TCoeff`×4096 | Rate-distortion opt: for each coeff group, compute cost(level) for level ∈ {0,1,2,...} | Branchy + compute: per-coeff rate estimation via CABAC context table. Bottleneck: branch mispredict + L2→L3 walks. |
| | `m_pdCostCoeff[4096]` | 32 KB | `double` | RD cost per candidate level | Large stride writes indexed by scanPos. Evicts m_blk/m_tmp from L2. |
| | `m_pdCostSig[4096]` | 32 KB | `double` | RD cost for significance | Same |
| | `m_pdCostCoeff0[4096]` | 32 KB | `double` | RD cost for level=0 | Same |
| | `m_rateIncUp[4096]` | 16 KB | `int` | Rate delta for up-round | Read-modify-write: load rate, add delta, store. Sequential scan. |
| | `m_rateIncDown[4096]` | 16 KB | `int` | Rate delta for down-round | Same |
| | `m_sigRateDelta[4096]` | 16 KB | `int` | Significance rate delta | Same |
| | `m_deltaU[4096]` | 16 KB | `TCoeff` | Quantized cand (level−1) | Write then read: written by rate estimation, read by trellis. |
| | `m_fullCoeff[4096]` | 16 KB | `TCoeff` | Full-precision coeffs | Same |
| 6. DepQuant trellis | `m_trellis[4096][2]` | 197 KB | `Decisions` | Viterbi trellis: state[i] ← min(state[i-1][0], state[i-1][1]) + cost | Sequential pointer-chase: each iteration reads prev state + cost. Cache stride = sizeof(Decisions). |
| | `RateEstimator::m_memory` | 33 KB | `uint8_t` | CABAC rate lookup tables | Table lookups: small random reads. Fits L2 alone. |
| 7. Inv transform | `m_blk`, `m_tmp` (reused) | 32 KB | `TCoeff`×4096 | Same butterfly as step 3 (inverse) | Same compute profile, but coefficients cold (evicted by RDOQ). |
| 8. Reconstruction | `cs.m_reco` sub-buf | 8 KB | `Pel`×4096 | `reco[i] = pred[i] + resi[i]` | Store stream. 1 load + 1 store. |

### Peak concurrent memory per TU pipeline pass

```
Prediction (Pel×4096):        8 KB    (cs.m_pred sub-buf, PelStorage pic-level)
Residual   (Pel×4096):        8 KB    (cs.m_resi sub-buf, PelStorage pic-level)
Reconstruct(Pel×4096):        8 KB    (cs.m_reco sub-buf, PelStorage pic-level)
m_blk      (TCoeff×4096):    16 KB    (TrQuant member, heap)
m_tmp      (TCoeff×4096):    16 KB    (TrQuant member, heap)
m_plTempCoeff (TCoeff×4096): 16 KB    (TrQuant member, heap)
QuantRDOQ arrays (×9):      288 KB    (QuantRDOQ members, embedded)
DepQuant trellis:            197 KB    (DepQuant member, embedded)
RateEstimator:                33 KB    (DQIntern member, embedded)
LFNST in/out:                 384 B    (TrQuant embedded)
Stack temps:                  500 B    (DCT butterfly arrays)
─────────────────────────────────────
Per-TU pipeline peak:       ~591 KB
Picture-level coeffs:       ~50 MB   (4K, cold except sub-buf)
```

### Cache residency analysis

| Component | Size | Fits | Notes |
|-----------|------|------|-------|
| pred/resi/reco sub-bufs | 24 KB | **L1** (32 KB) | Always hot |
| m_blk, m_tmp, m_plTempCoeff | 48 KB | **L2** (256-512 KB) | Hot during transform |
| RateEstimator | 33 KB | **L2** | Fits per-thread |
| QuantRDOQ arrays | 288 KB | **L3 only** | ~3× typical L2 |
| DepQuant trellis | 197 KB | **L3 only** | ~2× typical L2 |
| **Combined per-thread** | ~591 KB | **L3** | Spills L2 on every TU |

### TU pipeline execution profile (estimated)

```
Compute-heavy (port 0/1 bound):        fwd/inv transform butterflies    ~30% of TU time
Memory-bound (L2→L3 walks):            QuantRDOQ arrays, trellis        ~40% of TU time
Store-streaming (L1 fill):             residual, reconstruction         ~10% of TU time
Branchy (frontend-bound):              QuantRDOQ rate estimation        ~15% of TU time
Scatter/gather (address-gen):          LFNST                            ~5% of TU time
```

### OoO execution impact

The critical constraint is the **ROB (Reorder Buffer) window** — typically 200-400 uops on x86. Every cache miss that misses L1 inserts a pending load into the ROB and consumes a load buffer (~72 entries on modern cores). When load buffers fill, the OoO window collapses to the length of the longest outstanding miss chain.

**With small arrays (hypothetical L2-fit ~90 KB):**
- Transform loads hit L1/L2 (~12 cycles)
- ROB stays ~60-70% full of actual work uops
- OoO engine overlaps butterfly iterations freely
- ~3-4 IPC sustained on transform inner loop

**With large arrays (current ~485 KB):**
- Transform loads miss L2 → L3 (~40 cycles)
- ROB fills with pending loads (~72 entries consumed)
- OoO window shrinks to ~130 uops of actual work
- Execution ports starve waiting for data
- ~0.5-1 IPC sustained on transform inner loop

### Buffer ownership model

- `AreaBuf<T>` / `UnitBuf<T>` — pure views, no ownership
- `CompStorage` / `PelStorage` — heap-owned, used for `cs.m_pred`, `cs.m_resi`, `cs.m_reco` (picture-sized)
- `TrQuant` members (`m_blk`, `m_tmp`, `m_plTempCoeff`, `m_mtsCoeffs[4]`) — heap, sized `MAX_TB_SIZEY * MAX_TB_SIZEY` (4096), reused across all TUs
- `QuantRDOQ` arrays — embedded class members, compile-time fixed at `MAX_TB_SIZEY * MAX_TB_SIZEY`
- `DepQuant::m_trellis` — embedded class member, fixed at `[4096][2]`
- Stack temps — DCT butterfly arrays (E/O/EE/etc), tiny, per-call

### Why QuantRDOQ arrays are fixed-size 4096

All temporary arrays in `QuantRDOQ` (`QuantRDOQ.h:161-169`), `QuantRDOQ2` (`QuantRDOQ2.h:116`) and `DepQuant` (`DepQuant.h:368`) are **compile-time fixed-size member arrays**:

```cpp
double  m_pdCostCoeff           [4096];   // 32 KB
double  m_pdCostSig             [4096];   // 32 KB
double  m_pdCostCoeff0          [4096];   // 32 KB
int     m_rateIncUp             [4096];   // 16 KB
int     m_rateIncDown           [4096];   // 16 KB
int     m_sigRateDelta          [4096];   // 16 KB
TCoeff  m_deltaU                [4096];   // 16 KB
TCoeff  m_fullCoeff             [4096];   // 16 KB
CtxTpl  m_tplBuf                [4096];   // ~40 KB (QuantRDOQ2)
DQIntern::Decisions m_trellis   [4096][2]; // ~197 KB
```

Rationale:
- **Zero runtime allocation overhead** — no new/delete per TU (would dominate at 4×4 TU)
- **Struct members** — part of `TrQuant` → `PerThreadRsrc`, one contiguous allocation
- **Indexing by scanPos** — valid for any TU size since scan always starts at 0

At `--preset fast`, average TU is 16×16 to 32×32, using only 256-1024 of 4096 entries. The unused 3000+ entries are initialized to zero but never touched — they just pollute cache.

## Threading Model

### Three levels of parallelism

| Level | Granularity | What runs in parallel |
|-------|------------|----------------------|
| *Frame-level (FPP)* | Whole frames | Multiple pictures encoded concurrently |
| *CTU-level (WPP/Wavefront)* | Coding Tree Units | CTU encode + filter stages across workers |
| *Sub-picture tasks* | MCTF blocks, ALF | Motion estimation, temporal filtering |

### Frame-level parallelism (FPP)

- Default: `min(numThreads, 4)` parallel frames
- Each frame gets one `EncPicture` from a pool
- Frame dispatch via `EncGOP::xGetProcessingLists()` with temporal-layer ordering
- Lower temporal layers encoded serially for determinism

### CTU-level parallelism (WPP)

Per-CTU state machine (`EncSlice::xProcessCtuTask`) with 11 pipeline stages:

```
CTU_ENCODE → RESHAPE_LF_VER → LF_HOR → SAO_FILTER → ALF_GET_STATISTICS →
ALF_DERIVE_FILTER → ALF_RECONSTRUCT → CCALF_* → FINISH_SLICE → PROCESS_DONE
```

Each stage has spatial dependency checks against neighbor CTUs via `processStates[]` array.

### Parallel-to-sequential transition by stage

| Stage | Parallelism | Constraint |
|-------|------------|------------|
| CTU_ENCODE | **High** — wavefront anti-diagonal | Left, top, top-right neighbors |
| RESHAPE_LF_VER | **Moderate** | Right + bottom-right must finish encoding |
| LF_HOR | **Weak** — column-sequential | Top neighbor must finish LF_HOR first |
| SAO_FILTER | **Tight** | All 6 neighbors must finish LF_HOR |
| ALF_GET_STATISTICS | **Single-threaded** | Only runs on last CTU per row (line 1132) |
| ALF_DERIVE_FILTER | **Single-threaded** | Runs once at `deriveFilterCtu` |
| ALF_RECONSTRUCT | Row-iterating single thread | Must wait for derive |
| CCALF_* | Single-threaded | Same pattern |

The hard sequential bottleneck at `EncSlice.cpp:1132`:
```cpp
if( ctuPosX == lastCtuColInTileRow ) {
    processStates[ctuRsAddr] = ALF_GET_STATISTICS;  // only 1 CTU per row progresses
} else {
    processStates[ctuRsAddr] = PROCESS_DONE;        // all others are done
}
```

### Thread pool

- `NoMallocThreadPool` with lock-free `ChunkedTaskQueue` (128-slot chunks)
- `findNextTask()` — linear scan, CAS-claim available slots
- 1 ms busy-wait, then block on condition variable
- Task dispatch via `addBarrierTask(func, param, counter, done, barriers, readyCheck)`

### CPU utilization

- Baseline (fast-5fr): **3.48/4 CPUs utilized** (87%)
- Default thread count: 4 (min(hw_concurrency, resolution-based))

## TU Inter-Dependencies

| Group | Parallel? | Why |
|-------|-----------|-----|
| Residual quadtree siblings | **Yes** | Disjoint buffers, CABAC context saved/restored per sub-TU (InterSearch.cpp:4106) |
| ISP sub-TUs (INTRA) | **No** — strictly sequential | Sub-TU(n+1) reads recBuf from sub-TU(n); CABAC state flows; last-TU CBF inferred (IntraPrediction.cpp:1685) |
| Cross-CU (encoding) | **Yes** (CTU-level WPP) | CTU boundary checks, TU-level data independent |
| Deblocking filter | **Yes** (order-irrelevant) | Read-only TU metadata iteration, 4×4 block raster filter |
| ALF statistics | **Yes** (per-CTU) | Already row-serialized |

## Complete CTU Encoding Call Chain

```
encodeCtu [SEQUENTIAL]
└── xCompressCtu [SEQUENTIAL]
    └── xCompressCU [SEQUENTIAL per block; RECURSIVE]
        ├── [NO-SPLIT MODE TESTS]
        │   ├── xCheckRDCostUnifiedMerge   ← Merge/Skip
        │   │   ├── SATD pass [PARALLEL SUB-BLOCKS]
        │   │   └── Full RD pass [PARALLEL SUB-BLOCKS]
        │   │       └── xEncodeInterResidual → encodeResAndCalcRdInterCU
        │   │           └── xEstimateInterResidualQT
        │   │               └── transformNxN → invTransformNxN → CABAC → dist [SEQUENTIAL]
        │   │
        │   ├── xCheckRDCostInter           ← Regular Inter ME
        │   │   └── predInterSearch
        │   │       ├── Uni ME per (refList,refIdx) [INDEPENDENT PER REF]
        │   │       │   └── xMotionEstimation (TZ search) [SEQUENTIAL]
        │   │       ├── Bi-pred iteration (4 iters) [SEQUENTIAL]
        │   │       └── xEncodeInterResidual [SEQUENTIAL PIPELINE]
        │   │
        │   └── xCheckRDCostIntra           ← Intra modes
        │       └── estIntraPredLumaQT
        │           ├── SATD mode pruning [PARALLEL SUB-BLOCKS]
        │           └── Full RD mode loop [INDEPENDENT PER MODE]
        │               └── xIntraCodingLumaQT → xIntraCodingTUBlock
        │                   ├── Intra pred → residual → fwd Xform → quant → inv Xform → reco → CABAC
        │
        └── [SPLIT MODE TESTS — mutually exclusive]
            └── xCheckModeSplit → xCheckModeSplitInternal
                └── Recursive sub-CU loop:
                    └── xCompressCU(child) [PARALLEL SUB-BLOCKS]
```

## Data Flow: CTU → CU → TU Hierarchy

```
CTU (128×128 or 64×64)
├── QT/BT/TT partition ──► CUs (variable sizes)
│   ├── INTRA: TU = CU shape (no further split)
│   │   └── addEmptyTUs() at CodingStructure.cpp:388
│   ├── INTER: TU via residual quadtree
│   │   └── xEstimateInterResidualQT() at InterSearch.cpp:3524
│   └── TU pipeline per component: pred → resi → xform → quant → inv → reco → CABAC
│
├── Each CU has ~58 fields in 328 B (CodingUnit struct)
├── Cache waste: 95%+ (hot loops access 1-2 of 58 fields)
└── Wavefront parallelism: anti-diagonal encoding up to min(wCtus, hCtus) CTUs
```

## Key Source Files

| File | Role |
|------|------|
| `source/Lib/CommonLib/QuantRDOQ.h` | RDOQ temporary arrays (10 × [4096]) |
| `source/Lib/CommonLib/DepQuant.h` | Dependent quantization trellis + state |
| `source/Lib/CommonLib/DQIntern.h` | Rate estimator + scan data structures |
| `source/Lib/CommonLib/TrQuant.h` / `.cpp` | Forward/inverse transform + quant dispatch |
| `source/Lib/CommonLib/CodingStructure.h` / `.cpp` | Per-picture coeff/reco/pred buffers |
| `source/Lib/CommonLib/Buffer.h` | AreaBuf, UnitBuf, PelStorage, CompStorage |
| `source/Lib/EncoderLib/EncSlice.cpp` | xProcessCtuTask — 11-stage CTU state machine |
| `source/Lib/EncoderLib/EncCu.cpp` | xCompressCU, xCheckModeSplit, mode decision |
| `source/Lib/EncoderLib/InterSearch.cpp` | xEstimateInterResidualQT, TZ search |
| `source/Lib/EncoderLib/IntraSearch.cpp` | ISP sub-TU processing, intra mode eval |
| `source/Lib/Utilities/NoMallocThreadPool.h` / `.cpp` | Thread pool + task queue |
| `source/Lib/CommonLib/x86/TrQuantX86.h` | SIMD DCT/DST butterfly implementations |
| `source/Lib/CommonLib/x86/QuantX86.h` | SIMD quant helper templates |
| `source/Lib/CommonLib/CommonDef.h` | MAX_TB_SIZEY = 64, MAX_TB_SIZE = 4096 |
| `source/Lib/CommonLib/x86/InterpolationFilterX86.h` | SIMD interpolation with prefetch |

## Optimization Phases Reference

| Phase | Gain | Est. Time | Complexity | Key File Changes |
|-------|------|-----------|------------|------------------|
| PGO | +3% (5fr) / +5-10% (50fr) | ~1 hr | Low | CMakeLists.txt, scripts/pgo/ |
| Devirtualization | <2% | ~2 hrs | Medium | No virtual dispatch in hot paths |
| SoA layout | +5% | ~4 hrs | High | CodingUnit struct (328 B → hot fields) |
| Prefetching | +1-3% | ~1 hr | Medium | InterSearch.cpp (xPatternSearch inner loop) |
| Wavefront threading | +40-50% | ~8 hrs | Very High | EncSlice state machine → row-level dispatch |
| QuantRDOQ array sizing | TBD | ~4 hrs | Medium | QuantRDOQ.h, DepQuant.h array declarations |

## Baseline TMAM (v1.14.0, fast-5fr, park_joy_832x480f50)

| Metric | Value |
|--------|-------|
| Retiring | 46.6% |
| Frontend Bound | 26.2% |
| Backend Bound | 13.7% |
| Bad Speculation | 13.5% |
| Branch mispred | 1.83% |
| LLC miss rate | 19.58% |
