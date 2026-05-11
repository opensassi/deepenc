# vvenc_unit_test — SIMD Optimized Kernel Unit Tests

## 1. Overview

Bit-exact unit tests for SIMD-optimized compute kernels. Each test compares a reference (scalar) implementation against an optimized (SIMD) implementation using randomly generated inputs across configurable dimensions, strides, and bit depths. Tests cover transform/quantization, filtering, distortion metrics, motion estimation, and interpolation.

**Tests applies to**:
- [`source/Lib/CommonLib/AdaptiveLoopFilter.spec.md`](../source/Lib/CommonLib/AdaptiveLoopFilter.spec.md) — ALF in-loop filter SIMD kernels
- [`source/Lib/CommonLib/MCTF.spec.md`](../source/Lib/CommonLib/MCTF.spec.md) — Motion-compensated temporal filter SIMD kernels
- [`source/Lib/CommonLib/RdCost.spec.md`](../source/Lib/CommonLib/RdCost.spec.md) — Rate-distortion cost functions (SAD, HAD, SSE, weighted)
- [`source/Lib/CommonLib/IntraPrediction.spec.md`](../source/Lib/CommonLib/IntraPrediction.spec.md) — Intra prediction angle interpolation SIMD
- [`source/Lib/CommonLib/InterPrediction.spec.md`](../source/Lib/CommonLib/InterPrediction.spec.md) — Inter prediction BDOF/DMVR gradient filtering SIMD
- [`source/Lib/CommonLib/DepQuant.spec.md`](../source/Lib/CommonLib/DepQuant.spec.md) — Dependent quantization (state machine, rate estimation) SIMD
- [`source/Lib/CommonLib/TrQuant.spec.md`](../source/Lib/CommonLib/TrQuant.spec.md) — Transform/quantization coefficient ops (copy, round/clip, fast core transforms)

## 2. Component Specs

### 2.1 AdaptiveLoopFilter — `m_filter7x7Blk`

**Tested in**: `vvenc_unit_test.cpp:3063` (`test_AdaptiveLoopFilter`)

- `check_one_filterBlk`: compares `m_filter7x7Blk[linearIndex]` for linear (0) and non-linear (1) modes
- Random coefficients from `m_fixedFilterSetCoeff`, random clip values for non-linear mode
- Dimensions: w ∈ {8,16,32,48,64,128}, h ∈ {8,16,24,32,64,112,128}
- Random src stride (w+8 .. 8K), random dst stride (w .. 8K)
- Virtual boundary handling with random vbCTUHeight

### 2.2 MCTF — Temporal Denoising Kernels

**Tested in**: `vvenc_unit_test.cpp:1510` (`test_MCTF`)

- `m_applyBlock`: 9×9 size combinations (4..64), random strides, 6/8 refs, 8/10 bit, chroma formats 400/420
- `m_applyFrac` 4-tap and 6-tap interpolation: 16×16 motion vector positions, full 8K stride, chroma/luma channels
- `m_applyPlanarCorrection`: sizes 4..32, random stride, motion error 1..32
- `m_motionErrorLumaFrac8` (low-res 4-tap and full-res 8-tap): 8×8..64×64, random stride
- `m_motionErrorLumaInt8`: 8×8..64×64, random stride
- All functions: 100 randomized test cases per config

### 2.3 RdCost — Distortion Functions

**Tested in**: `vvenc_unit_test.cpp:2099` (`test_RdCost`)

- `m_wtdPredPtr` (lumaWeightedSSE): 1..128 × 2..128, random strides, 10-bit, luma weights array
- `m_fxdWtdPredPtr` (fixWeightedSSE): same dimensions, random fixed weight
- `m_afpDistortFunc[0][DF_SADw]` (SAD): w∈{1,2,4,8,16,32,64,128} × h∈{2,4,8,16,32,64,128}, subShift 0/1
- `m_afpDistortFunc[0][DF_HADw]` and `DF_HAD_fast` (HAD): same dimensions, fast and full variants
- `m_afpDistortFunc[0][DF_SAD_WITH_MASK]` (GEO weighted SAD): GEO partition masks, all valid split modes
- `dmvrSadX5` (DMVR SAD×5): 8×8 and 16×16, with/without centre position

### 2.4 IntraPrediction — `IntraPredAngleLuma`

**Tested in**: `vvenc_unit_test.cpp:843` (`test_IntraPred`)

- `IntraPredAngleLuma`: cubic and non-cubic interpolation modes
- Dimensions: 4..64 in power-of-2 steps
- Random deltaPos and intraPredAngle (16..128)
- 10-bit reference samples, 100 random cases per config

### 2.5 InterPrediction (InterPredInterpolation) — BDOF/DMVR

**Tested in**: `vvenc_unit_test.cpp:1750` (`test_InterPred`)

- `xFpBiDirOptFlow`: 8×16, 16×8, 16×16, 8/10 bit, random dst stride
- `xFpBDOFGradFilter` and `xFpProfGradFilter`: 4..32 × 4..32, random strides
- `xFpPadDmvr`: padSize 1/2, specific width/height combinations

### 2.6 DepQuant — Dependent Quantization State Machine

**Tested in**: `vvenc_unit_test.cpp:765` (`test_DepQuant`)

- `m_checkAllRdCostsOdd1`: all `ScanPosType` (ISC_SBB, SOC_SBB, EOC_SBB), random 4-bit and 31-bit coefficient distributions, sensitive close-call sum cases
- `m_updateStates`: random `ScanInfo` with sbbSize 4/16, various prevId/absLevel combinations, verifies state fields (rdCost, numSig, refSbbCtxId, tplAcc, sum1st, absVal, remRegBins, ctx)
- `m_updateStatesEOS`: end-of-subblock state update with CommonCtx level pointers, random prevId including cross-state references (prevId >= 4)
- `m_findFirstPos`: zero-out threshold modes, random coefficient distributions with not-found edge cases

### 2.7 TCoeffOps (TrQuant) — Transform Coefficient Operations

**Tested in**: `vvenc_unit_test.cpp:1120` (`test_TCoeffOps`)

- `cpyCoeff4/cpyCoeff8`: copy prediction residue (signed 11-bit) to aligned coefficient buffer, srcStride ≥ width
- `roundClip4/roundClip8`: round and clip to signed 16-bit with output bit depths 8/10, random 22-bit signed inputs
- `fastInvCore`: inverse transform core for transform sizes 4,8,16,32,64; random input coefficients and TMatrixCoeff (8-bit); reducedLines in multiples of 4
- `fastFwdCore_2D`: forward 2D transform for same transform sizes; random shift (1..16); cutoff in multiples of 4

### 2.8 Additional Test Suites

**Tested in**: `vvenc_unit_test.cpp:3167`

- `AffineGradientSearch::EqualCoeffComputer`: 6-parameter and 4-parameter affine modes, widths/heights 16..128, signed 13-bit inputs, random/minmax generators
- `PelBufferOps::addAvg`: no-stride and strided modes, sizes 4..192, random strides up to MAX_CU_SIZE, 8/4-wide SIMD variants
- `InterpolationFilter`: 8-tap, 6-tap, 4-tap, 2-tap horizontal/vertical filters; WxH combined filters (4×4, 8×H, 16×H); chroma/luma; filterCopy with/without biMCForDMVR; bilinear 2-tap 2D; weighted geometric blend (`m_weightedGeoBlk`)
- `SampleAdaptiveOffset::calcSaoStatisticsBo`: 8/10 bit, width/height 32/64/128, skip lines

## 3. Testing

- **Type**: Bit-exact SIMD correctness comparison
- **Framework**: Custom compare helpers (`compare_value`, `compare_values_1d`, `compare_values_2d`), optional tolerance
- **Pass condition**: All reference vs. optimized outputs match exactly (or within tolerance of 1 for MCTF applyBlock)
- **Failure mode**: Stderr mismatch report, subsequent suite continues, final exit(EXIT_FAILURE)
- **Random seeding**: Deterministic via command-line `--seed`, default = current time
- **Test selection**: `--testcase <name>` for individual suite, `--fast` for smaller dimensions
