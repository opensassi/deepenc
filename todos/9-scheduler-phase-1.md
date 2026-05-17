# scheduler-phase-1

GitHub Issue: https://github.com/opensassi/deepenc/issues/9

## Previous Work

### What Succeeded

- Full specification of 7 Scheduler module spec files merged (commit dab1f43)
- Wavefront DAG layer added for CTU-level spatial dispatch (commit fa328fb)
- All 14 artifact validations passing (10 mermaid + 4 D3 filmstrips)
- Spec files are authoritative for class declarations, method signatures, design rationale

### What Was Tried

- N/A — Phase 1 is first implementation step

### What Remains

- Implement `WorkUnit.h` (Stage enum 0-22, DepType constants, spatial CTU fields)
- Implement `RingBuffer.h/.cpp` (lock-free slot pool: init, alloc via CAS+bitmask, free, getFreeCount, getCapacity)
- Implement `TUPipelineDAG.h/.cpp` (DAG builder from MockTU list → linked WorkUnits)
- Implement `TUScheduler.h/.cpp` (dispatcher with submitModeTrial, BatchPolicy, xSubmitReady, xOnComplete)
- Implement `SchedulerTrace.h/.cpp` (binary trace writer: magic 0x53434854, record types 0-3)
- Implement TraceLoader, ExecutorStubs, SchedulerBench
- Unit tests, CMake build system, trace capture points in IntraSearch/InterSearch

### Key Technical Details

- `WorkUnit.h`: namespace vvenc, Stage enum 0-22 (_COUNT=23), SPATIAL_LEFT=1/TOP=2/TOP_RIGHT=4/BOT_RIGHT=8
- RingBuffer: slot=MAX_TB_SIZEY²×sizeof(TCoeff)=16384B, 8 slots default. Free mask as uint64_t[] with atomic head
- TUScheduler: uses `NoMallocThreadPool` from Utilities, batch policies via switch in xSubmitReady
- Trace format: header=magic(4B)+version(4B), record=type(1B)+reserved(1B)+stageCount(2B)+payloadSize(4B)+payload
- SHA-256: prefer mbedtls, no OpenSSL dependency
- All spec files at `source/Lib/Scheduler/*.spec.md` — implementation goes alongside
