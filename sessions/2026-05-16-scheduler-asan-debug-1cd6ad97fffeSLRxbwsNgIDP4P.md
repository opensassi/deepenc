**Session ID:** `2026-05-16-scheduler-asan-debug`

**Date / Duration:** 2026-05-16; prompter active ≈ 3.0 hours

**Project / Context:**
Debugging and fixing the VVenC H.266/VVC encoder's scheduler dispatch module (issue #14), which produced header-only output (425–466 bytes vs 112KB baseline) when the scheduler dispatch hooks were enabled. The scheduler module attempted TU-level pipeline parallelism via a WorkUnit DAG with 7-stage executors, but the implementation had multiple bugs causing encoder state corruption.

**Top-Level Component:**
Fixed scheduler dispatch corruption — issue #14 resolved with fall-through dispatch hook pattern. Three distinct bugs found and fixed.

**Second-Level Modules:**
- Off-by-one fix in `TUPipelineDAG_CU.cpp:69` — 1-based `pTu->idx` vs 0-based `tus[]` vector index
- Output propagation fix in `IntraSearch.cpp:1311` — `m_tuId + 1 == tu.idx` comparison
- Dispatch hook redesign — replaced WorkUnit/DAG/executor path with fall-through to normal TU pipeline
- InterSearch dispatch hook fix — same fall-through pattern applied
- ASAN build setup — `-fsanitize=address` CMake configuration with `-O1` optimization
- ASAN heap corruption diagnosis — SAO statistics buffer underflow traced to wrong TU pointers
- Stage executor decomposition design (`TuStageData`, 5 stage executors) — proved infeasible without deeper encoder refactoring
- Pipeline parallelism analysis — quantified the ~27 KB cross-stage state (CABAC, lambda scaling, TU mutation)

**Prompter Contributions:**
- Directed the debugging strategy: ASAN build first, then hypothesis-driven debug plan
- Chose the correct diagnostic approach (printf instrumentation for rapid feedback)
- Identified the pipeline parallelism refactoring as the right long-term goal
- Called for detailed breakdown of the cross-stage state coupling

**Model Contributions:**
- Set up ASAN cmake build with proper flags (`-O1`, `-Wno-error=maybe-uninitialized`)
- Wrote 5 stage executor implementations covering all code paths (ISP, MIP, LMCS, jointCbCr)
- Implemented the fall-through dispatch hook pattern that proved bit-exact
- Quantified cross-stage state at ~27 KB across 6 categories
- Built and tested multiple iterations (ASAN, release-sched, bench-scheduler.sh)
- Designed the `TuStageData` struct, DAG executor signatures, and cross-stage data flow

**Prompter Time Estimate:**
- Reading and digesting model responses: ~1.2 hours
- Thinking, strategizing, and weighing options: ~1.0 hours
- Writing messages and directives: ~0.8 hours
- **Total: ~3.0 hours**

**Model-Equivalent SME Time Estimate:**
~40 hours — a senior C++ video codec engineer would need approximately:
- ASAN/instrumented build setup: 2 hours
- Hypotheses formulation and debug plan: 4 hours
- Setting breakpoints, running GDB, analyzing state: 8 hours
- Diagnosing off-by-one and output propagation bugs: 4 hours
- Verifying and fixing the fall-through pattern: 6 hours
- Designing and implementing stage executors: 10 hours
- Pipeline parallelism refactoring analysis: 4 hours
- Testing and verification (A/B, bench suite): 2 hours

**Required SME Expertise:**
- C++14 performance-critical video codec development (VVenC/VVC internals)
- AddressSanitizer configuration and heap corruption analysis
- GDB debugging of multi-threaded encoder pipelines
- CABAC context modeling and probability estimation infrastructure
- SIMD optimization (AVX2/SIMDe) for video codec primitives
- CMake build system engineering with conditional compilation flags
- Software pipeline parallelism design (data flow decomposition, ring buffers)
- VVC/H.266 intra prediction and transform/quantization pipeline

**Aggregation Tags:**
scheduler-dispatch, asan-debugging, heap-corruption, vvenc, vvc, encoder-infrastructure, pipeline-parallelism, cabac-state, off-by-one, stage-decomposition
