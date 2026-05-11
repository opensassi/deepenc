# vvencapp — Standalone CLI Encoder Application

## Overview

`vvencapp` is the standalone command-line encoder application. Its `main()` entry point parses CLI arguments via `parseCfg()`, opens input YUV and output bitstream files, calls the VVenC encoder library loop (`vvenc_encode`), and prints timing statistics.

## Application Flow

```mermaid
graph TB
    Start(["main(argc, argv)"])
    LogCB["vvenc_set_logging_callback"]
    InitCfg["vvenc_init_default(...)<br/>VVEncAppCfg (easy mode)"]
    ParseCfg["parseCfg()"]
    ShowVer["show version/help?"]
    CreateEnc["vvenc_encoder_create()"]
    OpenEnc["vvenc_encoder_open(enc, &cfg)"]
    PrintCfg["print configuration"]
    OpenOut["open output bitstream file"]
    AllocAU["alloc access unit payload"]
    AllocYUV["alloc YUV input buffer"]

    subgraph EncodeLoop [Encoding Loop per Pass]
        InitPass["vvenc_init_pass(enc, pass)"]
        OpenInput["open YUV input file"]
        LoopBody["while (!bEof || !bEncodeDone)"]
        ReadFrame["read YUV frame"]
        Encode["vvenc_encode(enc, yuv, &au, &done)"]
        WriteAU["write bitstream"]
        Stats["print stats"]
        LoopBody --> ReadFrame --> Encode --> WriteAU --> Stats --> LoopBody
    end

    CloseOut["close output / input"]
    Summary["vvenc_print_summary(enc)"]
    CloseEnc["vvenc_encoder_close(enc)"]
    FreeBuf["free buffers"]
    Done(["return 0"])

    Start --> LogCB --> InitCfg --> ParseCfg
    ParseCfg --> ShowVer
    ShowVer -->|"showHelp/showVersion"| Done
    ShowVer --> CreateEnc
    CreateEnc --> OpenEnc
    OpenEnc --> PrintCfg --> OpenOut --> AllocAU --> AllocYUV
    AllocYUV --> EncodeLoop
    EncodeLoop --> CloseOut --> Summary --> CloseEnc --> FreeBuf --> Done
```

## CLI Argument Parsing Sequence

```mermaid
sequenceDiagram
    participant Main as main()
    participant Parser as parseCfg()
    participant AppCfg as VVEncAppCfg
    participant LibCfg as vvenc_config

    Main->>Main: vvenc_init_default(&cfg, W, H, fps, RC, QP, preset)
    Main->>Parser: parseCfg(argc, argv, appCfg, cfg)
    Parser->>AppCfg: appCfg.parse(argc, argv, &cfg, stream)
    alt --help
        AppCfg-->>Parser: m_showHelp=true
        Parser-->>Main: true
    else --version
        AppCfg-->>Parser: m_showVersion=true
        Parser-->>Main: true
    else parse error
        AppCfg-->>Parser: parserRes < 0
        Parser-->>Main: false
    end

    alt additional options (--options)
        Parser->>LibCfg: vvenc_set_param(&cfg, key, value)
        LibCfg-->>Parser: OK / BAD_NAME / BAD_VALUE
    end

    Parser->>LibCfg: vvenc_init_config_parameter(&cfg)
    Parser->>AppCfg: appCfg.checkCfg(&cfg, stream)
    Parser-->>Main: ret
```

## Key Functions

| Function | Description |
|---|---|
| `main(argc, argv)` | Entry point; orchestrates init → encode loop → cleanup |
| `parseCfg(...)` | Parses CLI args via `VVEncAppCfg::parse()`, handles `--help`/`--version`, applies additional settings |
| `msgFnc(...)` | Global logging callback (va_list) |
| `msgApp(...)` | Convenience wrapper for `msgFnc` |
| `printVVEncErrorMsg(...)` | Format and print VVenC error codes |
| `changePreset(...)` | Callback for preset mode changes |
