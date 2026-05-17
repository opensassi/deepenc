# scheduler-asan-debug

GitHub Issue: https://github.com/opensassi/deepenc/issues/14

## Previous Work

### What Succeeded

- Issue #13 closed: scheduler Phase 2a/2b/2c (intra dispatch, inter dispatch, thread pool wiring)
- `sched_pipeline_bench` — A/B microbenchmark (single-frame 1080p, runtime gate, dispatch_hits counter)
- ctx output pointer fix — pDist, pNumSig, pPred, checkCrossCPrediction propagated to WorkUnit
- CU DAG builder cross-TU linking fix
- `g_pSchedulerTraceTarget` cleared in `TUScheduler::destroy()` (dangling pointer)
- `g_schedulerDispatchCount` reset at each `VVEncImpl::init()`
- Stage guard in execIntraTu — only processes TU on RECONSTRUCT stage (eliminates 6/7 redundant calls)
- `scripts/bench-scheduler.sh` — automated A/B bit-exactness verification
- Bit-exact verified: `--preset slow` at 720p and 1080p

### What Was Tried

- TU_SEQUENTIAL policy: no effect on 418-byte output (H3 rejected)
- Removing stage guard: no effect (H3 rejected)
- Null executors for non-matching TU/comp: no effect (H2 rejected)
- Disabling dispatch hooks entirely (`&& false` guard): confirmed hooks ARE the cause — correct 214KB when disabled
- Config matching between `vvenc_init_default(..., VVENC_SLOW)` and `--preset slow`: unknown field difference
- Previous ASAN attempt failed due to LTO conflicts

### What Remains

- Root cause of **418-byte output** in dispatch path at 1920×1080 with `vvenc_init_default` config is **UNKNOWN**
- ASAN build and run is the next diagnostic step
- Config field difference between `vvenc_init_default` and `--preset slow` unidentified
- Potential fix areas:
  1. DAG builder pool overflow (estimatePoolSize too small for certain CU configs)
  2. Use-after-free in xOnComplete or dispatch cleanup
  3. Double-free of TempCtx, IntraTuExecCtx, or m_pDependents arrays
  4. Coeff buffer overflow in xIntraCodingTUBlock when called via executor

### Key Technical Details

- Working config: `vvenc_init_default(...,0,-1,VVENC_MEDIUM)` then `vvenc_init_preset(VVENC_SLOW)` — 214,631 bytes output
- Broken config: `vvenc_init_default(...,0,32,VVENC_SLOW)` — 418 bytes output (both scheduler ON)
- 3,946,367 dispatch_hits in broken config
- Most likely corruption: TUPipelineDAG_CU.cpp:60-86 (DAG builder writes past bounds), IntraSearch.cpp:1329-1335 (ctx double-free), TUScheduler.cpp:313-317 (xOnComplete double-free)
- ASAN build: `-DCMAKE_BUILD_TYPE=Debug -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF -fsanitize=address -O1 -g`
