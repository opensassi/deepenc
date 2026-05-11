# vvencinterfacetest — C API Encoder Interface Smoke Test

## 1. Overview

Validates the public C encoder API (create, open, encode, close) with synthetic YUV input across single-threaded, multi-threaded, and 1-pass rate control configurations. Exercises the full encode-flush lifecycle.

**Tests applies to**:
- [`source/Lib/vvenc/vvenc.cpp`](../source/Lib/vvenc/vvenc.cpp) — C API wrappers (create/open/close/encode)
- [`source/Lib/vvenc/vvencCfg.cpp`](../source/Lib/vvenc/vvencCfg.cpp) — Configuration initialization and parameter adaptation
- [`source/Lib/vvenc/vvencimpl.cpp`](../source/Lib/vvenc/vvencimpl.cpp) — Internal implementation bridging C API to EncLib

## 2. Component Specs

### 2.1 vvenc — Encoder API Lifecycle

**Tested in**: `vvencinterfacetest.c:67` (`run`), `vvencinterfacetest.c:220` (`main`)

- Calls `vvenc_encoder_create()` / `vvenc_encoder_open()` / `vvenc_encode()` / `vvenc_encoder_close()` in sequence
- Validates encoder creation returns non-null handle
- Validates `vvenc_encoder_open` returns 0 with valid config
- Validates `vvenc_encode` returns 0 and produces access units with non-zero payload
- Validates `vvenc_encoder_close` returns 0
- Validates `vvenc_get_last_error` content on failure paths

### 2.2 vvencCfg — Configuration Initialization

**Tested in**: `vvencinterfacetest.c:235` (`vvenc_init_default`), `vvencinterfacetest.c:99` (`vvenc_get_config`)

- Calls `vvenc_init_default()` to populate config struct
- Calls `vvenc_get_config()` after open to retrieve adapted config (parameters adjusted by encoder init)
- Tests with `m_numThreads = 0` (no threading) and `m_numThreads > 0` (multi-threading)
- Tests with `m_RCNumPasses = 1` for rate control mode
- Tests with `m_verbosity = VVENC_WARNING`
- Uses `vvenc_get_config_as_string` at VVENC_INFO verbosity

### 2.3 YUV Buffer and Access Unit Management

**Tested in**: `vvencinterfacetest.c:107`, `vvencinterfacetest.c:121`

- `vvenc_YUVBuffer_default()` / `vvenc_YUVBuffer_alloc_buffer()` — allocate input picture
- `vvenc_accessUnit_default()` / `vvenc_accessUnit_alloc_payload()` — allocate output bitstream container
- Synthetic flat-luma content: sets pixel value = min(frame, bitDepth max)
- `vvenc_YUVBuffer_free_buffer()` / `vvenc_accessUnit_free_payload()` — cleanup in `cleanup:` label

### 2.4 Encode Loop and Flushing

**Tested in**: `vvencinterfacetest.c:128`

- Encodes `maxFrames` input frames in a `for` loop
- Each frame: sets input buffer, calls `vvenc_encode`, checks `encodeDone` flag
- Flush phase: calls `vvenc_encode` with `NULL` input until `encodeDone == true`
- Verifies AU count matches expected (no gaps, no spurious empty payloads)
- Supports early termination (`runTillFlushed == false`) for RC test where encoder may produce fewer AUs than input frames

### 2.5 Test Configurations

**Tested in**: `vvencinterfacetest.c:220`

| # | Mode | Threads | RC | maxFrames | runTillFlushed |
|---|------|---------|----|-----------|----------------|
| 1 | Single-threaded | 0 | Off | 16 | true |
| 2 | Multi-threaded | auto | Off | 16 | true |
| 3 | Multi-threaded, long | auto | Off | 2*GOPSize+8 | true |
| 4 | 1-pass RC | auto | 1 | 2*GOPSize+8 | false |

## 3. Testing

- **Type**: Integration / smoke test
- **Framework**: Custom main() with return-code pass/fail
- **Pass condition**: All encode/decode cycles return 0, AU counts match, no error messages printed
- **Failure mode**: Non-zero return, message to stderr/stdout
- **No external dependencies** beyond vvenc shared library
