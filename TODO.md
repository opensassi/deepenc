# Technical Specification Generation Plan

## Workflow Per File

For each source file, run in sequence:

1. `generate sub-module spec <ClassName>` — produces `source/Lib/<Module>/<ClassName>.spec.md`
2. `node scripts/extract-artifacts.js --file source/Lib/<Module>/<ClassName>.spec.md` — extracts mermaid + D3
3. (if D3 present) `npm run verify-animation -- --file source/Lib/<Module>/.artifacts/<ClassName>.spec.md/d3-animation.html`
4. `npm run test-artifacts` — validates mermaid renders + filmstrip

---

## Phase 1: CommonLib (39 files — innermost dependency)

| # | File | D3? | Description |
|---|---|---|---|
| 1 | `BitStream` | | writable bitstream accumulator |
| 2 | `Mv` | ✓ | motion vector (DONE — reference template) |
| 3 | `dtrace` | | debug trace/logging infrastructure |
| 4 | `TimeProfiler` | | performance timing/profiling |
| 5 | `StatCounter` | | statistics counter infrastructure |
| 6 | `ProfileLevelTier` | | VVC profile/level/tier constraint tables |
| 7 | `Rom` | | scan orders, weight tables, ROM lookup tables |
| 8 | `RomTr` | | transform coefficient/derivative tables |
| 9 | `Contexts` | | CABAC probability model tables |
| 10 | `Unit` | | CU/TU data structures |
| 11 | `UnitPartitioner` | | CTU/CU/TU recursive partitioning |
| 12 | `ContextModelling` | | CABAC context derivation |
| 13 | `Buffer` | | pixel buffer operations |
| 14 | `Picture` | | picture buffer management |
| 15 | `Slice` | | slice and parameter set structures |
| 16 | `PicYuvMD5` | | MD5 hash of reconstructed YUV |
| 17 | `SEI` | | SEI message abstraction layer |
| 18 | `CodingStructure` | | frame-level coding data structure |
| 19 | `InterpolationFilter` | | luma/chroma interpolation kernels |
| 20 | `RdCost` | | distortion computation (SAD/SSE/HAD) |
| 21 | `Quant` | | scalar quantization/dequantization |
| 22 | `DepQuant` | | dependent scalar quantization |
| 23 | `QuantRDOQ` | | RDO-based quantization |
| 24 | `QuantRDOQ2` | | enhanced RDO quantization |
| 25 | `TrQuant` | | forward/inverse transform and quantization |
| 26 | `TrQuant_EMT` | | EMT transform coefficient ops |
| 27 | `SampleAdaptiveOffset` | | SAO filter |
| 28 | `AdaptiveLoopFilter` | | ALF filter |
| 29 | `LoopFilter` | | deblocking filter |
| 30 | `IntraPrediction` | | intra prediction |
| 31 | `InterPrediction` | | inter prediction + BDOF/PROF |
| 32 | `MatrixIntraPrediction` | | MIP (matrix-weighted intra prediction) |
| 33 | `AffineGradientSearch` | | affine motion estimation gradient search |
| 34 | `MCTF` | | motion compensated temporal filter |
| 35 | `Reshape` | | LMCS data structures |
| 36 | `SearchSpaceCounter` | | encoder search space statistics |
| 37 | `UnitTools` | | CU/PU/TU helper functions |
| 38 | `CommonDefX86` | | x86 CPUID extension detection |
| 39 | `InitX86` | | per-module SIMD function pointer init |

## Phase 2: Utilities (1 file)

| # | File | D3? | Description |
|---|---|---|---|
| 40 | `NoMallocThreadPool` | | lock-free thread pool for wavefront parallelism |

## Phase 3: EncoderLib (25 files — depends on CommonLib)

| # | File | D3? | Description |
|---|---|---|---|
| 41 | `BinEncoder` | | binary arithmetic encoder |
| 42 | `VLCWriter` | | variable-length coding writer |
| 43 | `CABACWriter` | | CABAC syntax element writer |
| 44 | `NALwrite` | | NAL unit output |
| 45 | `SEIwrite` | | SEI message serialization |
| 46 | `SEIEncoder` | | SEI message initialization |
| 47 | `GOPCfg` | | GOP structure configuration |
| 48 | `EncCfg` | | encoder configuration struct |
| 49 | `BitAllocation` | | visual activity / bit allocation |
| 50 | `RateCtrl` | | rate control logic |
| 51 | `EncHRD` | | HRD parameter computation |
| 52 | `EncSampleAdaptiveOffset` | | SAO encoder |
| 53 | `EncAdaptiveLoopFilter` | | ALF encoder |
| 54 | `EncReshape` | | LMCS reshape encoder |
| 55 | `IntraSearch` | | intra mode search |
| 56 | `InterSearch` | | inter mode search |
| 57 | `EncCu` | | CU-level encoding |
| 58 | `EncModeCtrl` | | mode decision control |
| 59 | `EncSlice` | | slice-level encoding |
| 60 | `EncPicture` | | picture-level encoding |
| 61 | `EncGOP` | | GOP-level encoding |
| 62 | `PreProcess` | | input picture padding |
| 63 | `SEIFilmGrainAnalyzer` | | film grain analysis |
| 64 | `EncLib` | ✓ | top-level encoder library |

## Phase 4: DecoderLib (1 file)

| # | File | D3? | Description |
|---|---|---|---|
| 65 | `DecCu` | | CU-level decoding |

## Phase 5: VVenC API (3 files — depends on all above)

| # | File | D3? | Description |
|---|---|---|---|
| 66 | `vvencCfg` | | public encoder configuration struct |
| 67 | `vvenc` | | external C API implementation |
| 68 | `vvencimpl` | ✓ | internal encoder wrapper |

## Phase 6: Applications (3 files)

| # | File | D3? | Module | Description |
|---|---|---|---|---|
| 69 | `vvencapp` | | vvencapp | CLI main entry point |
| 70 | `EncApp` | | vvencFFapp | FF encoder application class |
| 71 | `encmain` | | vvencFFapp | FF CLI main entry point |

---

## Phase 7: Test Files (with cross-references)

For each test file, generate a `.spec.md` that cross-references the source specs it tests.

| # | Test File | Cross-References To |
|---|---|---|
| T1 | `test/vvencinterfacetest/vvencinterfacetest.c` | `vvenc`, `vvencimpl`, `vvencCfg` |
| T2 | `test/vvenclibtest/vvenclibtest.cpp` | `EncLib`, `EncPicture`, `Slice`, `CodingStructure` |
| T3 | `test/vvenc_unit_test/vvenc_unit_test.cpp` | `AdaptiveLoopFilter`, `MCTF`, `RdCost`, `IntraPrediction`, `InterPrediction`, `DepQuant`, `TrQuant` |

Cross-reference format in each test `.spec.md`:
```markdown
**Tests applies to**:
- [`source/Lib/CommonLib/Mv.spec.md`](../source/Lib/CommonLib/Mv.spec.md) — motion vector operations
```

---

## Phase 8: Module-Level Aggregate Specs

After all 71 per-file specs exist, generate one aggregate spec per module:

| Module | Aggregate Spec | Contents |
|---|---|---|
| CommonLib | `source/Lib/CommonLib/CommonLib.spec.md` | Arch diagram, cross-module sequence diagram, SIMD variant summary |
| EncoderLib | `source/Lib/EncoderLib/EncoderLib.spec.md` | Arch diagram, encode pipeline sequence |
| VVenC | `source/Lib/vvenc/vvenc.spec.md` | C API facade spec |

---

## Phase 9: Root `technical-specification.md` Update

Populate the Module Reference table and add the top-level C4 diagram showing module relationships.

---

## Validation Commands

After each file:
```bash
node scripts/extract-artifacts.js --file source/Lib/<Module>/<ClassName>.spec.md
# if D3 animation present:
npm run verify-animation -- --file source/Lib/<Module>/.artifacts/<ClassName>.spec.md/d3-animation.html
```

After every 5-10 files (or at module boundaries):
```bash
npm run test-artifacts
```

After all files complete:
```bash
npm run validate-all
```
