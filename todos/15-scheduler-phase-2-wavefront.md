# 15-scheduler-phase-2-wavefront

## Issue Reference
GitHub Issue: https://github.com/opensassi/deepenc/issues/15

## Previous Work
### What Succeeded
- Per-stage executor infrastructure (6 executors, TuStageData, TUPipelineDAG CU builder) implemented
- Dispatch hooks in IntraSearch.cpp and InterSearch.cpp (16M+ firings, no corruption)
- sched_pipeline_bench with A/B comparison and ASAN support
- Bit-exact verified: --preset slow at 1080p
- DAG builder correctly builds sequential TU chains
- xProcessCtuStage defined in EncSlice (syncs scheduler CtuStates to actual progress)

### What Was Tried
- Inline scheduler dispatch in xIntraCodingTUBlock caused 418-byte output corruption (commit a8e73df)
- Fixed by falling through to normal inline path (commit 2680994) — executor code left as dead code
- TU_SEQUENTIAL policy, stage guard removal, null executors all rejected as root causes
- Prior ASAN attempt failed due to LTO conflicts (resolved in this session)

### What Remains
- Dead SchedulerExecutors code (471 lines) — either remove or re-integrate
- EncSlice wavefront dispatch (xProcessCtuStage never called from compressSlice)
- Thread safety for executeWorkUnits + xOnComplete
- BatchPolicy enforcement in multi-threaded mode
- RingBuffer per-thread scratch allocation
- Comprehensive benchmarks (frame-level dispatch, multi-thread)

## Key Technical Details
- Files: source/Lib/Scheduler/SchedulerExecutors.{cpp,h}, TuStageData.h, TUPipelineDAG_CU.cpp
- Files: source/Lib/EncoderLib/EncSlice.cpp (xProcessCtuStage defined at line 886, not called)
- Files: source/Lib/EncoderLib/IntraSearch.cpp (hooks at line 1277/1547)
- Executor signatures: bool SchedulerExecutors::execInitPred(WorkUnit*, void*), etc.
- CTU stages: 11 executors from Stage::CTU_ENCODE(12) through Stage::CCALF_RECON(22) in WorkUnit.h
- TUScheduler.h: submitFrame(Slice&, Picture*, EncSlice*) and advanceFrame() public API
- Known crash: "jointCbCr (2 vs 3)" CABACWriter.cpp:2232 if TU fields stale between dispatches
- ASAN build: -DCMAKE_BUILD_TYPE=Debug -DVVENC_ENABLE_SCHEDULER_DISPATCH=ON -fsanitize=address -O1 -g -Wno-maybe-uninitialized
