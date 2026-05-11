# EncApp — FFmpeg-Style Encoder Application Class

## Overview

`EncApp` is the encoder application class used by the `vvencFFapp` variant. It wraps encoder configuration (`vvenc_config`), application configuration (`VVEncAppCfg`), YUV file I/O (`YuvFileIO`), and bitstream output. Main entry points are `parseCfg()` for CLI argument handling and `encode()` for the encoding loop.

## Class Structure

```mermaid
graph TB
    App["EncApp"]

    subgraph Members
        m_cfg["m_cEncAppCfg"]
        m_conf["m_vvenc_config"]
        m_ctx["m_encCtx"]
        m_yuvIn["m_yuvInputFile"]
        m_yuvRecon["m_yuvReconFile"]
        m_bs["m_bitstream"]
        m_eb["m_essentialBytes"]
        m_tb["m_totalBytes"]
    end

    subgraph Methods
        ctor["EncApp()"]
        parse["parseCfg()"]
        enc["encode()"]
        outAU["outputAU()"]
        outYuv["outputYuv()"]
        openIO["openFileIO()"]
        closeIO["closeFileIO()"]
        stats["printRateSummary()"]
    end

    App --> m_cfg
    App --> m_conf
    App --> m_ctx
    App --> m_yuvIn
    App --> m_yuvRecon
    App --> m_bs

    m_cfg -.-> parse
    App --> ctor
    App --> parse
    App --> enc
    App --> outAU
    App --> outYuv
    enc --> openIO
    enc --> closeIO
    enc --> stats
```

## Encoding Sequence

```mermaid
sequenceDiagram
    participant Caller as main() / encmain
    participant App as EncApp
    participant Cfg as VVEncAppCfg
    participant Lib as vvenc (C API)
    participant YUV as YuvFileIO

    Caller->>App: new EncApp()
    App->>App: vvenc_config_default(&m_vvenc_config)
    App-->>Caller: ready

    Caller->>App: parseCfg(argc, argv)
    App->>Cfg: parse(argc, argv, &m_vvenc_config)
    App-->>Caller: true / false

    Caller->>App: encode()
    App->>Lib: vvenc_encoder_create()
    App->>Lib: vvenc_encoder_open(m_encCtx, &m_vvenc_config)
    App->>App: openFileIO()
    App->>YUV: open(inputFile, ...)

    loop per picture
        App->>YUV: readYuvBuf(yuvBuffer)
        App->>Lib: vvenc_encode(m_encCtx, yuv, &au, &done)
        alt au has payload
            App->>App: outputAU(au)
            App->>m_bitstream: write(payload)
        end
    end

    App->>App: closeFileIO()
    App->>Lib: vvenc_encoder_close(m_encCtx)
    App->>App: printRateSummary(framesRcvd)
    App-->>Caller: ret

    Caller->>App: delete EncApp
```

## Key Design Decisions

| Decision | Rationale |
|---|---|
| **Separate `parseCfg` + `encode`** | Clean separation of configuration and execution; enables reuse in tests and alternative frontends |
| **`outputAU` returns byte count** | Caller can track bitstream size; `m_essentialBytes` / `m_totalBytes` for rate statistics |
| **`outputYuv` is static** | Matches C callback signature expected by `vvencEncoder` for reconstruction output |
| **`presetChangeCallback`** | Allows `--preset` to reinitialize config via `vvenc_init_preset` at any point during parsing |
