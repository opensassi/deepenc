# production-ready

GitHub Issue: https://github.com/opensassi/deepenc/issues/11

## Previous Work

### What Succeeded

- HWPreAnalyzer module: 14 files (HWPreAnalyzer facade, HWBitstreamParser, HWCuPartitionAnalyzer, HWPreAnalysisStage, bench harness, 42 unit tests)
- All 4 encoder integration points wired (EncCu split hints, InterSearch MV seeding, RateCtrl QP override, scene cut override)
- 2.38× speedup on uniform synthetic metadata, 2.06× on mixed
- `hw_pipeline_bench` with `--with-hw` A/B comparison mode
- 50% instruction reduction, +8% IPC, -21% branch miss rate

### What Was Tried

- `forceSplit`: Cascade failures at small CU sizes until 32×32 min guard added
- Depth=1-only `noSplit`: Only 8.5% speedup; all-depths achieved 2.38×
- Confidence thresholds: 0.8 was too high (no hints applied); 0.6 unlocked noSplit

### What Remains

- Real QuickSync metadata capture tool (ffmpeg + H.264 hardware parser)
- Threshold calibration against real metadata
- BD-rate measurement across QP 22-37
- InterSearch MV seeding benchmark (requires P/B frames)
- `forceSplit` path hardening for edge cases

### Key Technical Details

- Decision tree (HWCuPartitionAnalyzer.cpp): 5 branches with confidence 0.3-0.92
- EncCu integration at `EncCu.cpp:1066`: `currQtDepth >= 1 && !isBoundary && isLuma`
- `noSplit` min CU: 16×16, conf >= 0.6
- `forceSplit` min CU: 32×32, depth <= 2, conf >= 0.9
- Sidecar: CSV (version,w,h + per-frame fields) + binary (per-frame: gridSize, mbTypes, mvPairs)
- Pipeline bench: `test/hw_pipeline_bench/pipeline_bench.cpp` — single-frame slow 1920×1080
