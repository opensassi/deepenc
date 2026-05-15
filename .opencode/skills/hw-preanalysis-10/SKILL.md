---
name: hw-preanalysis-10
description: Implement QuickSync hardware pre-analysis module for VVenC H.266 encoder guidance (GitHub issue #10)
---

# Skill: hw-preanalysis-10

## Issue Reference

GitHub Issue: https://github.com/opensassi/deepenc/issues/10

## Dependencies

Requires: **git** — load via `skill git` for the rebase-based commit workflow.

## Background

Designed during an interactive system design session evaluating the feasibility of using a QuickSync hardware encode (H.264) to produce metadata for guiding a downstream H.266/VVenC encode. Four integration points were identified: scene cut flags for GOPCfg, frame complexity for RateCtrl (QP initialization), CU split hints for EncCu early-skip, and MV fields for InterSearch TZ search seeding.

Key exploration outcomes:
- **File-backed approach** chosen over integrated VAAPI to prove value first without build dependency on libva/libmfx
- **H.264 16x16 MB granularity** too coarse for direct H.266 128x128 CTU mapping — 2D aggregation heuristic designed instead (MV variance + partition entropy + motion boundary detection)
- **HW reconstructed frame reference** evaluated and rejected — HW artifacts degrade H.266 reference quality; more useful as visual activity input
- **Replacing software lookahead** evaluated but deferred due to RateCtrl TRCPassStats coupling risk (incremental +15-18pp gain estimated if pursued)
- **Cascaded with existing ML module** (FASTSplitPredictor) for best results — HW filters easy cases at high confidence, ML handles complex cases, full RDO as safety net
- **Estimated speedup**: 22-27% conservative (HW-only), 35-45% aggressive (accept 1-2% BD-rate loss), 50-65% cascaded with ML

### What Remains
- Implement all 4 source files (.h/.cpp) in source/Lib/HWPreAnalysis/
- Write ffmpeg-based metadata extractor script (gen_hw_metadata.py)
- Add VVENC_ENABLE_HW_PREANALYSIS CMake option and conditional compilation guards
- Add m_hwPreAnalysis and m_hwMetadataPath fields to vvencCfg.h
- Wire HWPreAnalyzer::init() into VVEncImpl::init()
- Insert HWPreAnalysisStage before PreProcess in EncLib::initPass()
- Implement RateCtrl consumption (getFrameComplexity in initRateControlPic)
- Implement GOPCfg consumption (getSceneCut for intra period)
- Implement EncCu consumption (getCUSplitHint in xCompressCU)
- Implement InterSearch consumption (getMVField for TZ seeding)
- Add --hw-pre-analysis and --hw-metadata flags to vvencapp CLI
- Create test metadata sidecar files
- Write and register all unit tests
- Profile against software-only baseline to measure actual speedup

### Key Technical Details
- Facade: HWPreAnalyzer (singleton, vvenc namespace)
- Query interface: getFrameMetadata(poc, &meta), getFrameComplexity(poc, &float), getSceneCut(poc, &bool), getCUSplitHint(ctuX, ctuY, cuSize, &hint), getMVField(poc, &grid, &w, &h)
- Metadata struct: HWFrameMetadata { int m_iPOC, HWFrameType m_eFrameType, int m_iQP, uint64_t m_uBits, bool m_bSceneCut, float m_fMVComplexity, MBPartitionGrid m_cMBGrid }
- Split hint: CUSplitHint { bool m_bForceSplit, bool m_bNoSplit, CUSplitType m_eSplitType, float m_fConfidence }
- Heuristic thresholds: MV var < 0.05 + entropy < 0.1 => noSplit (conf 0.9); boundary > 4pel => forceSplit (conf 0.8)
- Sidecar format: JSON frontmatter (version, width, height, frames[]) + binary grids (uint8 MB types + int16_t MV pairs per frame)
- Pipeline insertion: EncLib::initPass() stage list, inserted before PreProcess
- Build guard: VVENC_ENABLE_HW_PREANALYSIS (CMake option, default OFF)
- Singleton pattern follows FASTSplitPredictor

## Persona

Senior C++ video encoding systems engineer with expertise in VVenC/VVC encoder architecture, QuickSync/VAAPI hardware encoding, and SIMD optimization. Familiar with the deepenc fork's conventions (m_/x prefix, EncStage pipeline, singleton pattern, int return codes).

## On Activation

1. Read source/Lib/HWPreAnalysis/HWPreAnalyzer.spec.md for the aggregate facade spec
2. Read source/Lib/HWPreAnalysis/HWBitstreamParser.spec.md for sidecar format and parser spec
3. Read source/Lib/HWPreAnalysis/HWCuPartitionAnalyzer.spec.md for split hint heuristic spec
4. Read source/Lib/HWPreAnalysis/HWPreAnalysisStage.spec.md for EncStage integration spec
5. Read the existing FASTSplitPredictor module (source/Lib/MLTools/) for singleton pattern reference
6. Read EncStage.h for the pipeline stage interface
7. Follow the implementation phases below

## Commands

- `setup` — create directory structure, add CMake option, add config fields
- `impl-core` — implement HWBitstreamParser, HWCuPartitionAnalyzer, HWPreAnalyzer facade
- `impl-stage` — implement HWPreAnalysisStage and pipeline insertion code
- `impl-consumers` — implement RateCtrl/GOPCfg/EncCu/InterSearch integration
- `impl-cli` — add CLI flags to vvencapp
- `test-build` — build with VVENC_ENABLE_HW_PREANALYSIS=ON, run unit tests
- `profile` — profile against software-only baseline with perf stat

## Files Reference

| File | Role |
|------|------|
| source/Lib/HWPreAnalysis/HWPreAnalyzer.spec.md | Aggregate facade spec (class decl, arch diagram, seq diagram, test matrix, CLI wiring) |
| source/Lib/HWPreAnalysis/HWBitstreamParser.spec.md | Parser spec (JSON + binary sidecar loader, format spec, yyjson recommendation) |
| source/Lib/HWPreAnalysis/HWCuPartitionAnalyzer.spec.md | Analyzer spec (5-branch decision tree, 4 helper functions, 28 tests) |
| source/Lib/HWPreAnalysis/HWPreAnalysisStage.spec.md | Stage spec (EncStage override, metadata attachment, lifecycle tests) |
| source/Lib/HWPreAnalysis/HWPreAnalyzer.h/.cpp | Facade singleton with query interface |
| source/Lib/HWPreAnalysis/HWBitstreamParser.h/.cpp | JSON + binary sidecar parser |
| source/Lib/HWPreAnalysis/HWCuPartitionAnalyzer.h/.cpp | 2D aggregation heuristic for CU split hints |
| source/Lib/HWPreAnalysis/HWPreAnalysisStage.h/.cpp | EncStage subclass for pipeline metadata attachment |
| source/Lib/EncoderLib/EncStage.h | Abstract base for pipeline stages |
| source/Lib/EncoderLib/EncLib.cpp | initPass() stage registration |
| source/Lib/vvenc/vvencimpl.cpp | VVEncImpl::init() module wiring |
| include/vvenc/vvencCfg.h | Config struct (add fields near m_mlEnable, line 741) |
| source/App/vvencapp/vvencapp.cpp | CLI flag parsing |
| source/Lib/MLTools/FASTSplitPredictor.h/.cpp | Singleton pattern reference |
| test/hw_preanalysis/hw_preanalysis_test.cpp | Unit test suite (all test IDs from 4 spec files) |
| scripts/gen_hw_metadata.py | Test metadata sidecar generator |

## Design Principles

- All new files — never modify regression baseline (4 files in technical-specification.md SS5)
- Guard all HW code with VVENC_ENABLE_HW_PREANALYSIS, zero codegen impact when OFF
- CU split hints are recommendations with confidence scores, never hard decisions
- Metadata sidecar failure degrades gracefully: encoder continues without HW guidance
- Follow existing encoder patterns (FASTSplitPredictor singleton, EncStage pipeline)
- File-backed first; VAAPI integration is a separate follow-up issue
