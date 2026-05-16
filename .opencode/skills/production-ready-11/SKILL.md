---
name: production-ready-11
description: Calibrate and production-harden the HWPreAnalysis module with real QuickSync metadata capture, threshold tuning, and BD-rate measurement (GitHub issue #11)
---

# Skill: production-ready-11

## Issue Reference

GitHub Issue: https://github.com/opensassi/deepenc/issues/11

## Dependencies

Requires: **git** — load via `skill git` for the rebase-based commit workflow.

## Previous Work

### What Succeeded
- HWPreAnalyzer module: 14 files (HWPreAnalyzer facade, HWBitstreamParser, HWCuPartitionAnalyzer, HWPreAnalysisStage, bench harness, 42 unit tests)
- All 4 encoder integration points wired (EncCu all-depth CU split hints, InterSearch MV seeding, RateCtrl QP override, scene cut override)
- 2.38x speedup on uniform synthetic metadata, 2.06x on mixed
- hw_pipeline_bench with --with-hw A/B comparison mode
- Perf stat: 50% instruction reduction, +8% IPC, -21% branch miss rate

### What Was Tried
- forceSplit: Cascade failures at small CU sizes until 32x32 min guard added
- Depth=1-only noSplit: Only 8.5% speedup; all-depths achieved 2.38x
- Confidence thresholds: 0.8 was too high (no hints applied); 0.6 unlocked noSplit

### What Remains
- Real QuickSync metadata capture tool (ffmpeg + H.264 hardware parser)
- Threshold calibration against real metadata
- BD-rate measurement across QP 22-37
- InterSearch MV seeding benchmark (requires P/B frames)
- forceSplit path hardening for edge cases

## Persona

Senior video encoding systems engineer with expertise in VVenC/VVC encoder architecture, QuickSync/VAAPI hardware encoding, and C++14 performance-critical code. Familiar with the deepenc fork's conventions (m_/x prefix, EncStage pipeline, singleton pattern, int return codes).

## On Activation

1. Read issue #11 for full context
2. Check for QuickSync/VAAPI hardware availability on current system
3. Check existing synthetic metadata at /tmp/hw_16fr/ and /tmp/hw_uniform/
4. Verify hw_pipeline_bench builds and runs with --with-hw
5. Begin implementation

## Commands

- `capture-metadata` — run ffmpeg QSV/VAAPI encode, extract per-MB data, produce CSV+bin sidecar files
- `tune-thresholds` — sweep confidence thresholds in HWCuPartitionAnalyzer.cpp and EncCu.cpp against real metadata, find optimal balance of speedup vs quality
- `measure-bdrate` — run hw_pipeline_bench at QP {22,27,32,37} with and without HW, compute BD-rate using PSNR and bitrate deltas
- `bench-inter` — encode a full GOP with P/B frames to benchmark InterSearch MV seeding
- `report-fix` — validate all traces pass, ensure bit-exact baseline, commit, close issue #11

## Key Technical Details

- Decision tree in `source/Lib/HWPreAnalysis/HWCuPartitionAnalyzer.cpp`: 5 branches
  - hasBound: conf=0.92
  - lowVar AND lowEntropy: conf=0.9 (noSplit)
  - medVar AND medEntropy: conf=0.6
  - highVar OR highEntropy: conf=0.85 (forceSplit QT)
  - else: conf=0.3
- EncCu integration at `source/Lib/EncoderLib/EncCu.cpp:1066`: `currQtDepth >= 1 && !isBoundary && isLuma`
- noSplit min CU: 16x16, conf >= 0.6
- forceSplit min CU: 32x32, depth <= 2, conf >= 0.9
- Sidecar format: CSV (version,w,h + per-frame: poc,frameType,qp,bits,sceneCut,mvComplexity) + binary (per-frame: gridSize:u32 + mbTypes:u8[W*H] + mvPairs:i16[W*H*2] interleaved)
- Pipeline bench: `test/hw_pipeline_bench/pipeline_bench.cpp` — single-frame slow preset 1920x1080, --with-hw flag

## Files Reference

| File | Role |
|------|------|
| `scripts/gen_hw_metadata.py` | Existing synthetic metadata generator (reference for expected CSV+bin format) |
| `source/Lib/HWPreAnalysis/HWCuPartitionAnalyzer.cpp` | Decision tree confidence thresholds to tune |
| `source/Lib/EncoderLib/EncCu.cpp` | Acceptance thresholds at line 1066 |
| `source/Lib/HWPreAnalysis/HWBitstreamParser.cpp` | CSV+binary sidecar parser |
| `test/hw_pipeline_bench/pipeline_bench.cpp` | Single-frame microbenchmark with --with-hw flag |
| `source/Lib/EncoderLib/InterSearch.cpp` | MV seeding at line 2057 |
| `source/Lib/EncoderLib/EncGOP.cpp` | RateCtrl QP override at line 624 |
| `source/Lib/EncoderLib/PreProcess.cpp` | Scene cut override at line 129 |

## Design Principles

- All new files — never modify regression baseline
- Guard all HW code with VVENC_ENABLE_HW_PREANALYSIS
- CU split hints are recommendations with confidence scores
- Metadata sidecar failure degrades gracefully
- BD-rate measurement must compare same-qp, not averaged
- QuickSync-capable hardware may not be available; provide fallback with pre-captured traces
