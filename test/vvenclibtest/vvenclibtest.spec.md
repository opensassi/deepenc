# vvenclibtest — Encoder SDK Integration Test

## 1. Overview

Exercises the VVenC encoder library as an SDK consumer: parameter validation (ranges, calling order, invalid input buffers), default behavior with rate control, string API parameter parsing, and timestamp/DTS correctness across frame-rate configurations.

**Tests applies to**:
- [`source/Lib/EncoderLib/EncLib.spec.md`](../source/Lib/EncoderLib/EncLib.spec.md) — Top-level encoder library orchestrating encode passes
- [`source/Lib/EncoderLib/EncPicture.spec.md`](../source/Lib/EncoderLib/EncPicture.spec.md) — Picture-level encoding and buffer management
- [`source/Lib/CommonLib/Slice.spec.md`](../source/Lib/CommonLib/Slice.spec.md) — Slice header encoding and parameter constraints
- [`source/Lib/CommonLib/CodingStructure.spec.md`](../source/Lib/CommonLib/CodingStructure.spec.md) — Frame-level coding data container

## 2. Component Specs

### 2.1 EncLib — Encoder Pass Lifecycle

**Tested in**: `vvenclibtest.cpp:236` (`testLibParameterRanges`), `vvenclibtest.cpp:1157` (`testLibCallingOrder`)

- `vvenc_check_config()` validates individual encoder parameters (calls into EncLib config validation)
- Valid parameter ranges: DecodingRefreshType, Level, Profile, Tier, GOPSize, Width, Height, IntraPeriod, QP, TargetBitRate, NumPasses, InputBitDepth, InternalBitDepth, TicksPerSecond, RPR, PicPartition
- Invalid parameter ranges produce expected failures via `expectedFail = true`
- Calling order validation: `vvenc_encoder_create()` → `vvenc_encoder_open()` → `vvenc_encode()` → `vvenc_encoder_close()`
- Invalid calling orders (uninit close, double open, encode without open) must fail
- `vvenc_init_pass()` multi-pass rate control sequences (1-pass, 2-pass)

### 2.2 EncPicture and Buffer Validation

**Tested in**: `vvenclibtest.cpp:1237` (`testInvalidInputParams`), `vvenclibtest.cpp:1196` (`inputBufTest`)

- Invalid input picture scenarios:
  - Uninitialized input picture (null planes)
  - Invalid picture sizes
  - Invalid luma stride (< width)
  - Invalid chroma stride
  - Invalid sample range (> bit-depth max)
- Each invalid case is expected to fail (`expectedFail = true`)
- Valid input with proper allocation (`invalidldInputBuf`) expected to succeed

### 2.3 Slice — Parameter Constraints

**Tested in**: `vvenclibtest.cpp:236` (`testLibParameterRanges`)

- `m_DecodingRefreshType` validated against `m_poc0idr` combinations (CRA, IDR, CDR, etc.)
- `m_IntraPeriod` tested with `m_poc0idr = -1, 0, 1` to exercise IDR/CRA/No-IDR flag logic
- `m_GOPSize` tested with various GOP configurations
- `m_QP` range validation (auto-QP, valid range, overflow)

### 2.4 CodingStructure — Encoding Data Flow (via SDK)

**Tested in**: `vvenclibtest.cpp:910` (`runEncoder`), `vvenclibtest.cpp:1031` (`checkTimestampsDefault`)

- Timestamp validation: CTS/DTS correctness across 9 frame-rate configurations (25/1 to 120000/1001)
- Tick-per-second modes: 90 kHz and 27 MHz
- DTS monotonicity: DTS must strictly increase per frame by expected CTS diff
- Missing-frame emulation: verifies DTS gap when frames are dropped
- User data passthrough via unstable API (`VVENC_USE_UNSTABLE_API`)

### 2.5 String API Interface

**Tested in**: `vvenclibtest.cpp:1179` (`testStringApiInterface`)

- `vvenc_set_param()` with valid key/value pairs (all VVENC_OPT_* constants)
- Bitrate string parsing: numeric, "1M", "1.5Mbps", "1000k", "1500.5kbps"
- Invalid string formats must return `VVENC_PARAM_BAD_NAME` or `VVENC_PARAM_BAD_VALUE`

## 3. Testing

- **Type**: API-level black-box integration test
- **Framework**: Custom `TEST()/TESTT()/ERROR()` macros with `g_numTests`/`g_numFails` counters
- **Pass condition**: `g_numFails == 0` after all test suites
- **Failure mode**: Non-zero exit code, verbose stderr on failure
- **Test selection**: Individual tests selectable by ID (1–6) via CLI argument
