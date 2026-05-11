# NoMallocThreadPool — Lock-Free Thread Pool for Wavefront Parallelism

## Overview

`NoMallocThreadPool` provides a lock-free, pre-allocated thread pool for wavefront-style parallelism in VVenC encoding. It avoids dynamic memory allocation during task execution by using a `ChunkedTaskQueue` of fixed-size `Slot` entries. Tasks are submitted via `addBarrierTask`, which supports optional barriers, wait-counters, and ready-check callbacks.

## Class Diagram

```mermaid
graph TB
    subgraph Synchronization
        Barrier["Barrier<br/>(atomic bool)"]
        BlockingBarrier["BlockingBarrier<br/>(Barrier + condvar + mutex)"]
        WaitCounter["WaitCounter<br/>(count + condvar + mutex)"]
    end

    subgraph TaskQueue
        ChunkedTaskQueue["ChunkedTaskQueue"]
        Chunk["Chunk<br/>(Slot[128])"]
        Slot["Slot<br/>(func, param, state, barriers)"]
        ChunkedTaskQueue --> Chunk
        Chunk --> Slot
    end

    subgraph ThreadPool
        NoMallocThreadPool["NoMallocThreadPool"]
        ThreadImpl["ThreadImpl<br/>(std::thread / PThread)"]
        NoMallocThreadPool --> ChunkedTaskQueue
        NoMallocThreadPool --> ThreadImpl
    end

    Slot --> Barrier
    Slot --> WaitCounter
    BlockingBarrier --> Barrier
    WaitCounter --> Barrier
```

## Task Execution Sequence

```mermaid
sequenceDiagram
    participant Client as Client Thread
    participant Pool as NoMallocThreadPool
    participant Queue as ChunkedTaskQueue
    participant Worker as Worker Thread

    Client->>Pool: addBarrierTask(func, param, barriers)
    alt single-threaded (no workers)
        Pool->>Pool: processTasksOnMainThread()
        Pool-->>Client: execute inline / return
    else multi-threaded
        Pool->>Queue: CAS slot from FREE → PREPARING
        Pool->>Slot: write func/param/barriers
        Pool->>Slot: state ← WAITING
        Pool-->>Client: return true
    end

    loop polling
        Worker->>Queue: findNextTask()
        alt slot.state == WAITING and barriers unblocked and readyCheck passes
            Worker->>Slot: CAS WAITING → RUNNING
            Worker->>Worker: execute func(threadId, param)
            alt done barrier provided
                Worker->>Barrier: unlock()
            end
            alt counter provided
                Worker->>WaitCounter: operator--()
            end
            Worker->>Slot: state ← FREE
        end
    end

    Client->>Pool: shutdown(block)
    Pool->>Worker: m_exitThreads ← true
    Worker-->>Pool: join
```

## Key Design Decisions

| Decision | Rationale |
|---|---|
| **Lock-freedom via CAS** | No `malloc` during encode loop; avoids priority inversion and jitter |
| **ChunkedTaskQueue** | Grows by appending 128-slot chunks; iterator wraps around for fair scheduling |
| **Barrier / BlockingBarrier** | Lightweight spin barrier for short waits; blocking fallback with `condition_variable` after `BUSY_WAIT_TIME` |
| **`ADD_TASK_THREAD_SAFE`** | Optional mutex around `m_nextFillSlot` for multi-producer scenarios |
| **`readyCheck` callback** | Enables conditional execution (e.g. only run when data dependency is resolved) |
