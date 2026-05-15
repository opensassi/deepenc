---
name: scheduler-phase-1-9
description: Implement WorkUnit, RingBuffer, TUPipelineDAG, TUScheduler classes + trace/bench harness (GitHub issue #9)
---

# Skill: scheduler-phase-1-9

## Issue Reference

GitHub Issue: https://github.com/opensassi/deepenc/issues/9

## Dependencies

Requires: **git** — load the git skill first via `skill git` for the rebase workflow.

## Previous Work

### What Succeeded
- Full specification of 7 Scheduler module spec files merged (commit dab1f43)
- Wavefront DAG layer added to support CTU-level spatial dispatch (commit fa328fb)
- All 14 artifact validations passing (10 mermaid + 4 D3 filmstrips)
- The spec files are authoritative references for class declarations, method signatures, and design rationale

### What Was Tried
- N/A — Phase 1 is first implementation step, no prior attempts at the C++ code

### What Remains
- Implement WorkUnit.h (Stage enum, DepType constants, WorkUnit struct with spatial CTU fields)
- Implement RingBuffer.h/.cpp (lock-free slot pool: init, alloc via CAS+bitmask, free, getFreeCount, getCapacity)
- Implement TUPipelineDAG.h/.cpp (DAG builder: takes mock TU list, creates linked WorkUnits, estimatePoolSize)
- Implement TUScheduler.h/.cpp (dispatcher: init, submitModeTrial, xSubmitReady, xOnComplete, BatchPolicy enum. submitFrame/advanceFrame stubbed for Phase 2)
- Implement SchedulerTrace.h/.cpp (binary trace writer: magic 0x53434854, record types 0-3, 8-byte headers)
- Implement TraceLoader.h/.cpp (binary trace reader, reconstructs DAG + root inputs from file)
- Implement ExecutorStubs.h/.cpp (SHA-256 reference implementation, stub executors for all Stage values)
- Implement SchedulerBench.cpp (CLI main: load trace, build DAG, dispatch with policy, verify hashes)
- Unit tests for each class in test/vvenc_unit_test/ (new .cpp file, registered via add_test in CMakeLists.txt)
- CMake build system: ENABLE_SCHEDULER_DISPATCH, ENABLE_SCHEDULER_TRACE options, conditional glob + compilation
- Trace capture points in IntraSearch.cpp / InterSearch.cpp within the EXISTING inline pipeline (not the scheduler dispatch path)
- End-to-end validation: encoder with ENABLE_SCHEDULER_TRACE=ON -> sched_trace.bin -> sched_bench --validate -> exit 0

### Key Technical Details
- WorkUnit.h: namespace vvenc, Stage enum values 0-22 (_COUNT=23), SPATIAL_LEFT=1/TOP=2/TOP_RIGHT=4/BOT_RIGHT=8, WorkUnit struct with m_depCount (std::atomic<int>)
- RingBuffer: slot size = MAX_TB_SIZEY * MAX_TB_SIZEY * sizeof(TCoeff) = 16384 bytes, default 8 slots. Free mask as uint64_t[] with atomic head for round-robin
- TUPipelineDAG: For Phase 1, accept a std::vector<MockTU> instead of real CodingUnit. MockTU has {width, height, compMask, mtsIdx, qp}. Creates 7-12 WorkUnits per MockTU depending on compMask
- TUScheduler: uses existing NoMallocThreadPool from Utilities. submitModeTrial() blocks via WaitCounter. Batch policies via switch(m_ePolicy) inside xSubmitReady()
- SchedulerTrace binary format: header = magic(4B) + version(4B). Record = type(1B) + reserved(1B) + stageCount(2B) + payloadSize(4B) + payload
- SHA-256: prefer mbedtls if available, otherwise standalone implementation. No OpenSSL dependency
- The `source/Lib/Scheduler/` directory already exists with `.spec.md` files. The implementation .h/.cpp files go alongside them
- IntraSearch.cpp: add ENABLE_SCHEDULER_TRACE blocks inside xIntraCodingTUBlock() at the same stage boundaries as the Stage enum. Record input buffers for INIT_PRED, PREDICT, RESIDUAL stages only

## Persona

You are a C++14 systems engineer implementing a decomposed encoder pipeline scheduler. You work spec-first — the design is fully documented in source/Lib/Scheduler/*.spec.md. Your task is to translate spec to code, following the conventions in technical-specification.md §5 (C++ Coding Conventions).

## On Activation

1. Read all spec files in `source/Lib/Scheduler/` to understand the exact class declarations
2. Read `technical-specification.md §5` for coding conventions
3. Read the existing `NoMallocThreadPool.spec.md` and implementation in `source/Lib/Utilities/` to understand the thread pool API
4. Check `source/Lib/CommonLib/CommonDef.h` for existing type definitions (ComponentID, Pel, TCoeff, MAX_TB_SIZEY)
5. Begin implementation per-phase following the commands below

## Commands

### `setup`
1. Read all spec files in `source/Lib/Scheduler/`
2. Check existing CMakeLists.txt structure in `source/Lib/vvenc/`
3. Verify `NoMallocThreadPool` header location and API
4. Report readiness

### `test-classes`
Compile and run unit tests for WorkUnit, RingBuffer, TUPipelineDAG, TUScheduler, SchedulerTrace:
```
cmake -B build_sched -DENABLE_SCHEDULER_DISPATCH=ON && make -j && ./sched_bench --test
```

### `trace-capture`
Build encoder with ENABLE_SCHEDULER_TRACE=ON and run one frame:
```
cmake -B build_trace -DENABLE_SCHEDULER_TRACE=ON && make -j
./bin/release-static/vvencapp -i test/data/park_joy.yuv -s 832x480 -f 1 --preset fast -o /dev/null
```

### `bench`
Run the bench harness on a captured trace:
```
./sched_bench sched_trace.bin --policy stage --window 8 --validate
```

### `report-fix`
1. Run full validation: `cmake -B build_trace ...` + `sched_bench --validate` — confirm exit 0
2. Verify no regressions: `Test_vvencinterfacetest` still passes (baseline unchanged)
3. Commit with `skill git` workflow (single atomic commit)
4. Close issue #9

## Files Reference

| File | Role |
|------|------|
| `source/Lib/Scheduler/WorkUnit.h` | Stage enum, DepType constants, WorkUnit struct |
| `source/Lib/Scheduler/RingBuffer.h` / `.cpp` | Lock-free slot-based intermediate buffer pool |
| `source/Lib/Scheduler/TUPipelineDAG.h` / `.cpp` | DAG builder: mock-TU -> WorkUnit[] with dep edges |
| `source/Lib/Scheduler/TUScheduler.h` / `.cpp` | Dispatcher: submitModeTrial, batch policies, completion |
| `source/Lib/Scheduler/SchedulerTrace.h` / `.cpp` | Binary trace writer |
| `source/Lib/SchedulerBench/TraceLoader.h` / `.cpp` | Binary trace reader |
| `source/Lib/SchedulerBench/ExecutorStubs.h` / `.cpp` | SHA-256 reference + stub executors |
| `source/Lib/SchedulerBench/SchedulerBench.cpp` | CLI main |
| `source/Lib/SchedulerBench/CMakeLists.txt` | Bench target build |
| `source/Lib/vvenc/CMakeLists.txt` | Conditional scheduler sources |
| `source/Lib/EncoderLib/IntraSearch.cpp` | Trace capture points |
| `source/Lib/EncoderLib/InterSearch.cpp` | Trace capture points |
| `test/vvenc_unit_test/*scheduler*` | Unit tests |
| `source/Lib/Utilities/NoMallocThreadPool.h` | Thread pool dependency |
| `source/Lib/CommonLib/CommonDef.h` | Type definitions (ComponentID, Pel, TCoeff) |

## Design Principles

- **C++14 only** — no C++17 features
- **namespace vvenc** — everything in the vvenc namespace
- **No inheritance** — plain classes with composition and forward declarations
- **Virtual destructor** — always present even for non-polymorphic classes
- **int return codes** — 0 = success, negative = error
- **m_ prefix** for members (m_p for pointers, m_b for bools)
- **x prefix** for private helpers
- **No encoder integration** — ENABLE_SCHEDULER_DISPATCH code is NOT wired into IntraSearch/InterSearch/EncSlice. Only ENABLE_SCHEDULER_TRACE capture points are added to the existing inline path
- **Standalone compilation** — WorkUnit.h and RingBuffer.h must compile with only `<atomic>`, `<cstdint>`, and CommonDef.h
- **Test first** — each class gets a unit test before the next class is started
