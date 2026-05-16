---
name: scheduler-completion-13
description: Complete scheduler encoder integration — fix DISPATCH crash, wire thread pool, add InterSearch + wavefront dispatch, production harden (GitHub issue #13)
---

# Skill: scheduler-completion-13

## Issue Reference

GitHub Issue: https://github.com/opensassi/deepenc/issues/13

## Dependencies

Requires: **system-design** — load the spec tree first via `skill system-design` for spec context on scheduler module.

## Previous Work

### What Succeeded
- All 7 scheduler class files implemented (WorkUnit, RingBuffer, TUPipelineDAG x2, TUScheduler, SchedulerExecutors, SchedulerTrace, PictureDAG)
- 13 unit tests passing for RingBuffer, TUPipelineDAG, TUScheduler, SchedulerTrace
- CABAC ctx snapshot/restore in execIntraTu (issue #12, commit ccd342d)
- TU_SEQUENTIAL dispatch policy enforcement in xSubmitReady
- sched_bench self-test passes and trace replay works
- CMake: conditional ENABLE_SCHEDULER_DISPATCH, scheduler sources compile for trace-only too
- End-to-end trace capture via VVENC_SCHED_TRACE env var

### What Was Tried
- submitModeTrial(CodingUnit\*) removed — was dead code (internal pool lost all executor function pointers); the hook + executeWorkUnits is the active path
- executeWorkUnits moved outside VVENC_SOURCE guard — was dead code because VVENC_SOURCE is never defined in the build system
- sched_bench changed from submitModeTrial to executeWorkUnits to fix silent executor skipping
- Cross-TU linking in DAG builder was attempted but reverted — TU_SEQUENTIAL is enforced via dispatch-time filtering instead

### What Remains
- Phase 2a: Fix DISPATCH crash (per-TU field init) + wire thread pool
- Phase 2b: InterSearch dispatch hook + functional inter executors
- Phase 2c: Frame-level wavefront dispatch in EncSlice
- Phase 3: Thread-safe dispatch, per-thread scratch, RingBuffer, BatchPolicy enforcement, benchmarks

## Persona

Senior C++ compiler engineer with deep expertise in H.266/VVC encoder internals, multithreaded pipeline design, and lock-free scheduling. Understands CABAC state machines, CU/TU partitioning, and wavefront parallel processing.

## On Activation

1. Show the 4-phase completion plan from the issue body
2. Check current build: `cmake -B build_sched -S . -DVVENC_ENABLE_SCHEDULER_DISPATCH=ON -DVVENC_ENABLE_SCHEDULER_TRACE=ON -DCMAKE_BUILD_TYPE=Release && cmake --build build_sched -j$(nproc) --target vvenc_scheduler_test --target vvencapp`
3. Run existing tests: `./bin/release-static/vvenc_scheduler_test`
4. Check current status of each phase's implementation
5. Report which phase is ready to start

## Commands

- `setup` — configure cmake + build all scheduler targets (vvenc, vvencapp, vvenc_scheduler_test, sched_bench)
- `test` — run scheduler unit tests + bench self-test
- `test-encode` — encode 1 frame with DISPATCH=ON to test for assertion failures
- `gdb-dispatch` — set breakpoints at IntraSearch dispatch hook + execIntraTu, single-step through DAG build + TU dispatch
- `fix-tu-state <strategy>` — apply per-TU field init fix (jointCbCr, compAlpha, CBFs) to execIntraTu
- `wire-pool` — add getThreadPool() accessor to EncLib + pass pool to g_pScheduler->init()
- `add-inter-hook` — mirror IntraSearch dispatch hook in InterSearch.cpp
- `add-frame-wavefront` — wire submitFrame/advanceFrame into EncSlice::compressSlice()
- `bench` — run 3-frame encode comparison (DISPATCH=ON vs OFF)
- `report-fix` — validate, push, close issue with commit hash

## Debugging Context

### Crash: "jointCbCr (2 vs 3)" at CABACWriter.cpp:2232

The assertion fires because the executor calls `xIntraCodingTUBlock` with a TU whose `jointCbCr` field was left at a stale value from a previous TU's processing. The inline callers set `currTU.jointCbCr = 0` immediately before calling `xIntraCodingTUBlock` (e.g., IntraSearch.cpp line 2075).

**Fix in execIntraTu (SchedulerExecutors.cpp, before calling xIntraCodingTUBlock):**
```cpp
pTu->jointCbCr = 0;
pTu->compAlpha = 0;
if (isChroma(compId)) { pTu->cbf[1] = 0; pTu->cbf[2] = 0; }
```

### Thread Pool Wiring

EncLib creates `m_threadPool` at EncLib.cpp:240:
```
m_threadPool = new NoMallocThreadPool(m_encCfg.m_numThreads, "EncSliceThreadPool", &m_encCfg);
```

Add accessor to EncLib.h: `NoMallocThreadPool* getThreadPool() const { return m_threadPool; }`

In VVEncImpl::init() (vvencimpl.cpp:~143), after `EncLib::initEncoderLib()` succeeds:
```cpp
if (!g_pScheduler)
{
    g_pScheduler = new TUScheduler();
    if (g_pScheduler->init(m_pEncLib->getThreadPool(), 8) < 0)
    { delete g_pScheduler; g_pScheduler = nullptr; }
}
```

### InterSearch Hook Location

`InterSearch::xEstimateInterResidualQT` at InterSearch.cpp:~1643. The residual encoding follows the same pattern as intra — CABAC ctx snapshot before TU loop, DAG builds from CU's TUs, executors restore ctx per TU call.

### Frame Wavefront Entry Point

`EncSlice::compressSlice(Picture* pic)` at EncSlice.cpp:537. The CTU processing loop is `xProcessCtus()` (line ~595). The scheduler path replaces this with:
```cpp
g_pScheduler->submitFrame(slice, pic);
while (g_pScheduler->advanceFrame() == 0) { /* yield or spin */ }
```

### CTU Stage Executors (11 stages)

| Stage | Enum Value | Function to Call |
|-------|-----------|-----------------|
| CTU_ENCODE | 12 | EncCu::encodeCtu(pic, qp, ctuX, ctuY) |
| RECON_WRITE | 13 | pic->writeBack(pelBuf) |
| LF_VER | 14 | LoopFilter::loopFilterPic(pic, EDGE_VER) |
| LF_HOR | 15 | LoopFilter::loopFilterPic(pic, EDGE_HOR) |
| SAO_FILTER | 16 | SampleAdaptiveOffset::SAOProcess(pic) |
| ALF_STATS | 17 | AdaptiveLoopFilter::calcStatistics(pic) |
| ALF_DERIVE | 18 | AdaptiveLoopFilter::deriveCoeffs(pic) |
| ALF_RECON | 19 | AdaptiveLoopFilter::reconstruct(pic) |
| CCALF_STATS | 20 | AdaptiveLoopFilter::calcCcAlfStatistics(pic) |
| CCALF_DERIVE | 21 | AdaptiveLoopFilter::deriveCcAlfCoeffs(pic) |
| CCALF_RECON | 22 | AdaptiveLoopFilter::reconstructCcAlf(pic) |

## Files Reference

| File | Role |
|------|------|
| source/Lib/Scheduler/SchedulerExecutors.h | IntraTuExecCtx struct + executor function declarations |
| source/Lib/Scheduler/SchedulerExecutors.cpp | execIntraTu impl — needs per-TU field init fix |
| source/Lib/EncoderLib/IntraSearch.cpp | Dispatch hook at xIntraCodingTUBlock (~line 1277) |
| source/Lib/EncoderLib/InterSearch.cpp | Needs dispatch hook at xEstimateInterResidualQT |
| source/Lib/EncoderLib/EncSlice.cpp | compressSlice() — needs wavefront dispatch path |
| source/Lib/EncoderLib/EncCu.cpp | encodeCtu/xCompressCU — needs recursive dispatch guard |
| source/Lib/EncoderLib/EncLib.h/.cpp | NoMallocThreadPool* m_threadPool — needs getThreadPool() accessor |
| source/Lib/vvenc/vvencimpl.cpp | VVEncImpl::init() — wire g_pScheduler->init with real pool |
| source/Lib/Scheduler/TUScheduler.h/.cpp | Dispatcher facade — thread-safe dispatch, BatchPolicy, scratch |
| source/Lib/Scheduler/PictureDAG.cpp | CTU wavefront DAG — needs m_pfnExec setup per stage |
| source/Lib/Scheduler/TUPipelineDAG_CU.cpp | CU DAG builder — fix chroma component dependency edges |
| source/Lib/Scheduler/RingBuffer.h/.cpp | Intermediate buffer pool — wire into executor stages |
| test/scheduler_test/scheduler_test.cpp | Add 5+ new tests for hook, frame dispatch, thread pool |

## Design Principles

- **Fix the crash first** (Phase 2a) — everything depends on DISPATCH=ON working without assertions
- **Thread pool must be real** — nullptr pool means synchronous dispatch; real parallelism requires NoMallocThreadPool
- **Bit-exactness is non-negotiable** — DISPATCH=ON must produce identical output to DISPATCH=OFF
- **Hook pattern is symmetric** — InterSearch hook mirrors IntraSearch exactly: ctx snapshot → DAG → executeWorkUnits → cleanup
- **Frame wavefront replaces inline loop** — EncSlice::compressSlice drops xProcessCtus and uses submitFrame + advanceFrame
- **Per-TU field init must match inline callers** — GDB the inline callers to dump all per-TU fields, replicate in executors
- **Default OFF** — ENABLE_SCHEDULER_DISPATCH stays off by default until all phases pass bit-exactness
- **Test each phase independently** — each phase adds integration tests before merging
