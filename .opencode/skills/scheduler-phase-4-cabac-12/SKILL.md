---
name: scheduler-phase-4-cabac-12
description: Fix CABAC state corruption in scheduler dispatch path and enable the ENABLE_SCHEDULER_DISPATCH pipeline hook (GitHub issue #12)
---

# Skill: scheduler-phase-4-cabac-12

## Issue Reference

GitHub Issue: https://github.com/opensassi/deepenc/issues/12

## Dependencies

Requires: **scheduler-phase-1-9** — load this skill first via `skill scheduler-phase-1-9` for the full scheduler architecture context.

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

- Direct per-TU-component executor: calls xIntraCodingTUBlock without CABAC ctx restore — triggers assertion jointCbCr (2 vs 1)
- Per-TU component filter (COMP_Y only): didn't address root cause (CABAC state is the issue)

### What Remains

- Fix CABAC context initialization in execIntraTu (snapshot TempCtx before DAG build, restore before each executor call)
- Uncomment the dispatch hook in IntraSearch.cpp
- Bit-exactness validation (md5sum comparison of output bitstream)
- Thread pool dispatch via addBarrierTask (needs non-capturing static callback)

## Persona

You are a C++14 systems engineer implementing a decomposed encoder pipeline scheduler. You work spec-first — the design is fully documented in source/Lib/Scheduler/*.spec.md. Your task is to translate spec to code, following the conventions in technical-specification.md §5 (C++ Coding Conventions).

## On Activation

1. Read this skill file fully
2. Read `source/Lib/EncoderLib/IntraSearch.cpp` around line 1277 to see the comment-blocked dispatch hook
3. Read `source/Lib/Scheduler/SchedulerExecutors.h/.cpp` for the executor infrastructure
4. Read `source/Lib/CommonLib/Contexts.h` for the TempCtx class definition
5. Begin implementation per-phase following the commands below

## Commands

### `setup`

```
cmake -B build_p4 -DVVENC_ENABLE_SCHEDULER_DISPATCH=ON -DVVENC_ENABLE_SCHEDULER_TRACE=ON && cmake --build build_p4 -j
```

### `test`

Run bit-exact comparison between DISPATCH=ON and DISPATCH=OFF:

```bash
# Produce both outputs
./bin/release-static/vvencapp -i test/data/park_joy_832x480f50.yuv -s 832x480 -f 3 --preset fast -o /tmp/out_dispatch.bin
# Compare with OFF (use existing build or rebuild with DISPATCH=OFF)
md5sum /tmp/out_dispatch.bin /tmp/out_baseline.bin
```

### `fix cabac-context`

Apply the CABAC context fix:

1. **Update `IntraTuExecCtx`** in `SchedulerExecutors.h`: add `void* pCtxStart` field (use `void*` to avoid requiring TempCtx definition in the header).

2. **Snapshot ctxStart in the dispatch hook** (`IntraSearch.cpp`, uncomment the block at ~line 1277):
   ```cpp
   const TempCtx ctxStart(m_CtxCache, m_CABACEstimator->getCtx());
   ```
   Store a heap copy in each IntraTuExecCtx:
   ```cpp
   ctx->pCtxStart = new TempCtx(m_CtxCache, ctxStart);
   ```

3. **Restore in execIntraTu** (`SchedulerExecutors.cpp`):
   ```cpp
   if (ctx->pCtxStart)
   {
       TempCtx* pCtx = (TempCtx*)ctx->pCtxStart;
       ctx->pSearch->m_CABACEstimator->getCtx() = *pCtx;
       ctx->pSearch->m_CABACEstimator->resetBits();
   }
   ```
   Note: execIntraTu needs access to IntraSearch's m_CABACEstimator. Since SchedulerExecutors is a friend of IntraSearch, this is accessible. But the header `IntraSearch.h` must be included in `SchedulerExecutors.cpp` (already done).

4. **Cleanup**: in the dispatch hook cleanup loop:
   ```cpp
   delete (TempCtx*)ctx->pCtxStart;
   ```

5. **Uncomment** the `#if ENABLE_SCHEDULER_DISPATCH` block in `IntraSearch.cpp`.

### `bench`

Run the scheduler bench harness:

```
./bin/release-static/sched_bench --test
```

### `report-fix`

1. Run `test` — confirm bit-exact
2. Run `bench` — confirm self-test passes
3. Run `setup` with DISPATCH=OFF — confirm no regression
4. Commit with `skill git` workflow

## Debugging Context

- Assertion: `ERROR: In function "joint_cb_cr" in CABACWriter.cpp:2232: wrong value of jointCbCr (2 vs 1)`
- Root cause: CABAC estimator context not reset between executor calls. Each TU pipeline call expects fresh CABAC state matching the point where xIntraCodingLumaQT would have called it in the sequential path.
- CABAC reset pattern in the sequential path:
  ```cpp
  const TempCtx ctxStart(m_CtxCache, m_CABACEstimator->getCtx());
  // ... per-TU loop ...
  m_CABACEstimator->getCtx() = ctxStart;
  m_CABACEstimator->resetBits();
  xIntraCodingTUBlock(tu, COMP_Y, ...);
  ```
- Guard globals:
  - `g_schedulerActive` (bool) — true when scheduler is dispatching
  - `tls_lastCu` (thread_local const void*) — last CU processed by scheduler
  - `tls_lastComp` (thread_local uint8_t) — last component processed by scheduler
- These are declared in `TUScheduler.h` and defined in `TUScheduler.cpp`
  - The tls variables are currently in `IntraSearch.cpp` inside the comment-blocked hook

## Files Reference

| File | Role |
|------|------|
| `source/Lib/Scheduler/SchedulerExecutors.h` | IntraTuExecCtx struct, SchedulerExecutors class |
| `source/Lib/Scheduler/SchedulerExecutors.cpp` | execIntraTu implementation |
| `source/Lib/EncoderLib/IntraSearch.cpp` | Dispatch hook at line ~1277 (comment-blocked) |
| `source/Lib/Scheduler/TUScheduler.h` | g_schedulerActive, g_pScheduler globals |
| `source/Lib/Scheduler/TUScheduler.cpp` | Global definitions |
| `source/Lib/Scheduler/WorkUnit.h` | WorkUnit struct with m_pCtx |
| `source/Lib/CommonLib/Contexts.h` | TempCtx class definition |
| `source/Lib/CommonLib/CodingStructure.h` | CodingStructure, TransformUnit types |
| `source/Lib/EncoderLib/IntraSearch.h` | IntraSearch class with friend SchedulerExecutors |
| `source/Lib/vvenc/vvencimpl.cpp` | g_pScheduler initialization |
| `test/vvenc_unit_test/scheduler_test.cpp` | 25 existing unit tests |

## Design Principles

- **C++14 only** — no C++17 features
- **namespace vvenc** — everything in the vvenc namespace
- **int return codes** — 0 = success, negative = error
- **m_ prefix** for members, **x prefix** for private helpers
- **No encoder behavioral change when DISPATCH=OFF** — the flag must default to OFF
- **CABAC state management** — TempCtx must be snapshot and restored per-executor
- **All 25 unit tests must pass** before declaring success
- **Bit-exactness is non-negotiable** — DISPATCH=ON output must match DISPATCH=OFF exactly
- **Per-TU-component not per-stage** — the executor calls the full xIntraCodingTUBlock, not individual stage functions. Per-stage executors would be a future optimization.
