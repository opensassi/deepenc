# SchedulerBench — Scheduler Replay & Benchmark Harness

## 1. Overview

`SchedulerBench` is a standalone executable that replays scheduler traces captured from a live encoder session. It loads a `sched_trace.bin` file, reconstructs the WorkUnit DAG with the recorded root-stage input data, dispatches through the scheduler under test, and verifies that every stage output matches the trace's expected hash.

The harness is not part of the main encoder build — it is a separate CMake target (`sched_bench`) built only when `ENABLE_SCHEDULER_DISPATCH=ON`.

**Dependencies**: `TUScheduler`, `TUPipelineDAG`, `WorkUnit`, `RingBuffer`, `TraceLoader`, `ExecutorStubs`.

**Lifecycle**: CLI-driven. `load trace → build DAG → run scheduler → verify hashes → report`.

## 2. CLI Interface

```
Usage: sched_bench <trace_file> [options]

Options:
  --policy <name>     Batching policy: tu|stage|hybrid  (default: stage)
  --window <n>        Batch window size in TUs           (default: 8)
  --threads <n>       Number of worker threads            (default: all available)
  --validate          Verify output hashes against trace  (default: true)
  --perf              Measure cycles per stage batch      (default: false)
  --iterations <n>    Number of replay iterations         (default: 1)
  --output <file>     Write perf results to JSON file

Exit codes:
  0   All assertions passed
  1   Hash mismatch detected
  2   Trace file parse error
  3   DAG construction failed
  4   Scheduler init failed
```

## 3. Trace Format

The trace binary format is specified in `SchedulerTrace.spec.md §2.2`. The harness reads:
- File header (magic + version)
- Record stream (stage records, output records, frame markers)
- Root stage input data for INIT_PRED, PREDICT, RESIDUAL stages

## 4. Verification Flow

```
sched_bench sched_trace.bin --policy stage --window 8

1. Load trace
   └─ TraceLoader::load("sched_trace.bin")
      ├─ validate file header
      ├─ iterate records
      ├─ store per-stage metadata
      └─ store root-stage input data

2. Build DAG
   └─ TUPipelineDAG::build(cu, mode, pool, ...)
      └─ use trace metadata to reconstruct TU tree
      └─ create WorkUnits with deferred executors

3. Run scheduler
   └─ TUScheduler::submitModeTrial(...)
      └─ dispatches using --policy / --window
      └─ ExecutorStubs compute SHA-256 per stage

4. Verify
   └─ Compare each stage's SHA-256 against trace hash
   └─ Compare final per-TU output hashes

5. Report
   └─ "432,000 stages verified. 0 mismatches. 142.3ms total."
   └─ If --perf: per-stage batch cycle counts + cache misses
```

## 5. Component Specifications

### 5.1 Class: `TraceLoader`

```cpp
#pragma once

#include <cstdint>
#include <cstdio>
#include <vector>

namespace vvenc {

struct WorkUnit;

struct TraceStage
{
    uint32_t tuId;
    uint8_t  stage;
    uint8_t  compId;
    int8_t   qp;
    uint8_t  width, height;
    uint8_t  mtsIdx;
    std::vector<uint8_t> inputData;   // empty for non-root
    uint8_t  outputHash[32];
};

struct TraceOutput
{
    uint32_t tuId;
    uint8_t  compId;
    uint8_t  finalHash[32];
};

class TraceLoader
{
public:
    int load(const char* pFilename);
    int getStageCount() const;
    int getOutputCount() const;
    const TraceStage* getStages() const;
    const TraceOutput* getOutputs() const;

private:
    std::vector<TraceStage> m_stages;
    std::vector<TraceOutput> m_outputs;
    int xReadHeader(FILE* pFile);
    int xReadRecord(FILE* pFile);
};

}
```

### 5.2 Class: `ExecutorStubs`

```cpp
#pragma once

#include <cstdint>

namespace vvenc {

struct WorkUnit;

class ExecutorStubs
{
public:
    /// Register stub executors: identical to real pipeline functions
    /// but compute SHA-256 of output buffers instead of actual processing.
    static int registerAll();

    /// Compute SHA-256 of a buffer
    static void sha256(const uint8_t* pData, int size, uint8_t hash[32]);

    /// Compare two hashes
    static bool hashEqual(const uint8_t a[32], const uint8_t b[32]);

    /// Executor for any stage: writes zeros to output, computes hash
    static bool stubExecutor(WorkUnit* pWu, void* pScratch);
};

}
```

## 6. Testing Requirements

### Integration Tests

| Test | What to Verify |
|------|----------------|
| `sched_bench --validate` | No hash mismatches on a known-good trace |
| `sched_bench --policy stage` vs `tu` | Different dispatch order, same final hashes |
| `sched_bench --window 4` vs `16` | Different window sizes, same hashes |
| `sched_bench --threads 1` vs `4` | Single vs multi-threaded, deterministic hashes |
| `sched_bench --iterations 100` | 100 runs produce identical results |
| `sched_bench corrupt trace` | Returns exit code 2 |
| `sched_bench --perf` | Per-stage timing data produced, non-zero |
| `sched_bench missing file` | Returns exit code 2 |
