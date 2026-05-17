# 16-scheduler-dispatch-reintegrate

## Issue Reference
GitHub Issue: https://github.com/opensassi/deepenc/issues/16

## Previous Work
### What Succeeded
- DAG builder fixed: `estimatePoolSize` now counts `MAX_NUM_TBLOCKS` always, not cbf-dependent
- `m_pDependents` converted from `WorkUnit**` (dynamic new/delete) to fixed `m_pDependents[MAX_DEPS=4]` inline array — eliminates double-free
- `xOnComplete` no longer calls `delete[] m_pDependents` — safe for multi-threaded use
- `xOnComplete` CTU state update scoped to CTU-level stages only (prevents intra-TU dispatch from corrupting frame CtuStates)
- `TuStageData::m_orgBuf` changed from `PelBuf` to `CPelBuf` (const-correct for `cs.getOrgBuf()`)
- DAG stages reduced from 7 to 5 per component (merged PREDICT→INIT_PRED, QUANT_FILL→FWD_XFORM)
- `TUPipelineDAG::wireExecutors()` added — maps DAG stages to `SchedulerExecutors` functions and binds `TuStageData` context
- PictureDAG RECON_WRITE stage removed (no real pipeline equivalent)
- `schedulerStageToTaskType()` / `taskTypeToCtuWfState()` added for proper Stage↔TaskType mapping
- `vvenc_scheduler_test` updated and passes (13/13 tests)
- All infrastructure changes compile with `VVENC_ENABLE_SCHEDULER_DISPATCH=ON`
- Full ctest suite: 37/38 pass (1 pre-existing Scalar comparison failure identical to baseline)

### What Was Tried
- IntraSearch executor dispatch in `xIntraCodingTUBlock`: builds 5-stage DAG → populates TuStageData → wireExecutors → executeWorkUnits. Crashes with `BitStream::write: Unsupported parameters`. Likely CABAC context init or stale TU field issue.
- InterSearch executor dispatch via `execInterTu`: single work unit × execInterTu. Disabled pending same fix.
- Wavefront dispatch: `xProcessCtus` calls `submitFrame` → loop `advanceFrame()`. Hangs because `xProcessCtuTask` state machine advances all CTUs round-robin (one stage per CTU per pass), but scheduler dispatches one CTU through all stages — breaks SAO/ALF neighbor dependencies. Also corrupts CtuStates when intra-TU dispatch overwrites frame state (fixed).
- xProcessCtuStage loop variant: tried driving full pipeline inside execCtuStage, but state machine ordering didn't match.

### What Remains
- Debug IntraSearch executor dispatch: BitStream "Unsupported parameters" crash. Suspect: `tu.jointCbCr`, `tu.cbf[]`, `tu.mtsIdx[]` not properly reset between dispatches. See issue #15 comment about "jointCbCr (2 vs 3)" at CABACWriter.cpp:2232.
- Debug InterSearch executor: same root cause likely.
- Fix wavefront dispatch: needs to either (a) decompose `xProcessCtuTask` into per-stage functions, or (b) have `execCtuStage` drive the round-robin loop internally by calling `xProcessCtuTask<false>` repeatedly until PROCESS_DONE.
- Multi-threaded wavefront: `xSubmitFrameReady` currently runs inline. For threaded mode, submit work units through `m_pPool->addBarrierTask()`.
- Verify `g_schedulerActive` flag correctly guards all dispatch entry points against recursive re-entry.
- Bit-exact verification: all modes with `--preset faster --frames 3` produce identical output to non-scheduler baseline.

## Key Technical Details
- IntraSearch hook: `IntraSearch.cpp:1277-1289` — flag-only mode active. Replace with actual dispatch using `TUPipelineDAG::build(TransformUnit*, uint8_t, ...)` + TuStageData.
- InterSearch hook: `InterSearch.cpp:4312-4322` — flag-only. Use `execInterTu` with `InterTuExecCtx`.
- Wavefront gate: `EncSlice.cpp:819` — `if (false && g_pScheduler && !vvencSchedulerDisabled() && m_pcEncCfg->m_numThreads == 0)` — change `false` to test condition.
- known crash path: `tu.jointCbCr` in `SchedulerExecutors::execFwdXform` at line 176-178 resets `tu.cbf[1] = 0; tu.cbf[2] = 0;` but does not reset `tu.jointCbCr` itself, which comes from CU-level state.
- Stage enum: `INIT_PRED=0, RESIDUAL=2, FWD_XFORM=3, INV_XFORM=9, RECONSTRUCT=10` (5 stages). CTU stages: `CTU_ENCODE=12` through `CCALF_RECON=22`.
- DAG stages per component: 5 (INIT_PRED, RESIDUAL, FWD_XFORM, INV_XFORM, RECONSTRUCT). Max dependents per work unit: 2 (intra-component chain + cross-component link).

## Experiment References
- `perf/experiments/` — not applicable (scheduler is concurrency infrastructure, not SIMD hot-path)
