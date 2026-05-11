# encmain — FFmpeg-Style Encoder Main Entry Point

## Overview

`encmain.cpp` contains the `main()` function for the `vvencFFapp` variant of the VVenC encoder. It parses SIMD options, creates an `EncApp` instance, delegates to `EncApp::parseCfg()` and `EncApp::encode()`, and reports timing statistics.

## Program Flow

```mermaid
graph TB
    Start(["main(argc, argv)"])
    LogCB["vvenc_set_logging_callback"]
    SIMD["parse --SIMD option<br/>vvenc_set_SIMD_extension"]
    CreateApp["new EncApp()"]
    ParseCfg["EncApp::parseCfg(argc, argv)"]
    ShowVer["isShowVersionHelp()?"]
    StartTime["record start time (steady_clock + system_clock + clock)"]
    Encode["EncApp::encode()"]
    EndTime["record end time (steady_clock + system_clock + clock)"]
    DeleteApp["delete EncApp"]
    ComputeTime["compute CPU time via clock() or GetProcessTimes()"]
    PrintTime["print total time (cpu + elapsed)"]
    Done(["return ret"])

    Start --> LogCB --> SIMD
    SIMD -->|"vvenc_set_SIMD_extension fails"| DoneErr(["return 1"])
    SIMD --> CreateApp --> ParseCfg
    ParseCfg -->|"parseCfg fails"| DoneErr2(["return 1"])
    ParseCfg --> ShowVer
    ShowVer -->|"show version/help"| Done0(["return 0"])
    ShowVer --> StartTime --> Encode --> EndTime --> DeleteApp --> ComputeTime --> PrintTime --> Done
```

## Sequence Diagram

```mermaid
sequenceDiagram
    participant Main as main()
    participant App as EncApp
    participant Lib as vvenc (C API)

    Main->>Main: vvenc_set_logging_callback(msgFnc)

    Main->>Main: parse --SIMD option via program_options
    Main->>Lib: vvenc_set_SIMD_extension(simdOpt)

    Main->>App: new EncApp()
    Main->>App: parseCfg(argc, argv)
    alt parse failed
        App-->>Main: false
        Main->>App: delete
        Main-->>Main: return 1
    else help/version
        App-->>Main: isShowVersionHelp()=true
        Main->>App: delete
        Main-->>Main: return 0
    end

    Main->>App: encode()
    Note over App: inside encode(): open encoder,<br/>encode loop, close encoder
    App-->>Main: ret

    Main->>Main: compute elapsed (steady_clock) and CPU time (clock / GetProcessTimes)
    Main->>Main: msgApp(VVENC_INFO, "Total Time: ...")
    Main-->>Main: return ret
```

## Key Responsibilities

| Responsibility | Implementation |
|---|---|
| SIMD extension override | Parses `--SIMD` flag via `program_options::scanArgv` before creating `EncApp` |
| Lifecycle management | Creates and destroys `EncApp`; handles early exit on parse error or `--help`/`--version` |
| CPU time measurement | Uses `clock()` on POSIX (sum of all threads), `GetProcessTimes()` on Windows for accurate per-thread CPU time |
| Elapsed time measurement | `std::chrono::steady_clock` for high-resolution wall-clock timing |
| Logging callback | Registers `msgFnc` as global callback via `vvenc_set_logging_callback` |
