# scheduler-completion

GitHub Issue: https://github.com/opensassi/deepenc/issues/13

## Previous Work

### What Succeeded

- All 7 scheduler class files implemented (WorkUnit, RingBuffer, TUPipelineDAG x2, TUScheduler, SchedulerExecutors, SchedulerTrace, PictureDAG)
- 13 unit tests passing for RingBuffer, TUPipelineDAG, TUScheduler, SchedulerTrace
- CABAC ctx snapshot/restore in execIntraTu (issue #12, commit ccd342d)
- TU_SEQUENTIAL dispatch policy enforcement in xSubmitReady
- `sched_bench` self-test passes, trace replay works
- CMake: conditional `ENABLE_SCHEDULER_DISPATCH`, scheduler sources compile for trace-only
- End-to-end trace capture via `VVENC_SCHED_TRACE` env var

### What Was Tried

- `submitModeTrial(CodingUnit*)` removed — dead code (internal pool lost executor function pointers)
- `executeWorkUnits` moved outside `VVENC_SOURCE` guard — was dead code (never defined in build)
- `sched_bench` changed from `submitModeTrial` to `executeWorkUnits` to fix silent executor skipping
- Cross-TU linking in DAG builder attempted but reverted (TU_SEQUENTIAL enforced via dispatch-time filtering)

### What Remains

- **Phase 2a**: Fix DISPATCH crash (per-TU field init) + wire thread pool
- **Phase 2b**: InterSearch dispatch hook + functional inter executors
- **Phase 2c**: Frame-level wavefront dispatch in EncSlice
- **Phase 3**: Thread-safe dispatch, per-thread scratch, RingBuffer, BatchPolicy enforcement, benchmarks

### Key Technical Details

- Crash: `"jointCbCr (2 vs 3)"` at CABACWriter.cpp:2232 — TU field stale from previous processing
- Fix in execIntraTu: set `pTu->jointCbCr = 0; pTu->compAlpha = 0;` plus chroma CBF reset before calling xIntraCodingTUBlock
- Thread pool: `EncLib` creates `m_threadPool` at `EncLib.cpp:240`, needs `getThreadPool()` accessor
- InterSearch hook at `InterSearch::xEstimateInterResidualQT` (~line 1643) — mirrors IntraSearch pattern
- Frame wavefront: `EncSlice::compressSlice()` replaces `xProcessCtus()` with `g_pScheduler->submitFrame()/advanceFrame()`
- 11 CTU stage executors (CTU_ENCODE=12 through CCALF_RECON=22)
