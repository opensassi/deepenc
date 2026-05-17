# scheduler-phase-4-cabac

GitHub Issue: https://github.com/opensassi/deepenc/issues/12

## Previous Work

### What Succeeded

- `g_pScheduler` global initialized at encoder startup (vvencimpl.cpp)
- `m_pCtx` added to WorkUnit for executor context passing
- SchedulerExecutors class with execIntraTu wrapper and setupIntraTu helper
- `g_schedulerActive` guard global for recursion safety
- CU-level dispatch hook coded in xIntraCodingTUBlock (comment-blocked, labeled "Phase 4 (deferred)")
- friend class SchedulerExecutors added to IntraSearch
- Full compile with DISPATCH=ON + TRACE=ON, encoder runs normally with hook disabled

### What Was Tried

- Direct per-TU-component executor: calls `xIntraCodingTUBlock` without CABAC ctx restore — triggers assertion `jointCbCr (2 vs 1)`
- Per-TU component filter (COMP_Y only): didn't address root cause

### What Remains

- Fix CABAC context initialization in execIntraTu (snapshot TempCtx before DAG build, restore before each executor call)
- Uncomment the dispatch hook in IntraSearch.cpp
- Bit-exactness validation (md5sum comparison of output bitstream)
- Thread pool dispatch via addBarrierTask (needs non-capturing static callback)

### Key Technical Details

- Assertion: `ERROR: joint_cb_cr in CABACWriter.cpp:2232: wrong value of jointCbCr (2 vs 1)`
- CABAC reset pattern in sequential path:
  ```cpp
  const TempCtx ctxStart(m_CtxCache, m_CABACEstimator->getCtx());
  // ... per-TU loop ...
  m_CABACEstimator->getCtx() = ctxStart;
  m_CABACEstimator->resetBits();
  xIntraCodingTUBlock(tu, COMP_Y, ...);
  ```
- Fix: snapshot ctxStart in dispatch hook, store heap copy in IntraTuExecCtx, restore in execIntraTu
- Guard globals: `g_schedulerActive` (bool), `tls_lastCu` (thread_local), `tls_lastComp` (thread_local)
- Hook location: IntraSearch.cpp:~1277 (comment-blocked `#if ENABLE_SCHEDULER_DISPATCH` block)
