# hw-preanalysis

GitHub Issue: https://github.com/opensassi/deepenc/issues/10

## Previous Work

### What Succeeded

- Interactive system design session completed evaluating QuickSync hardware pre-analysis feasibility
- Four integration points identified: scene cut flags (GOPCfg), frame complexity (RateCtrl QP), CU split hints (EncCu early-skip), MV fields (InterSearch TZ seeding)
- File-backed approach chosen over integrated VAAPI
- Cascaded with ML module (FASTSplitPredictor) for best results
- Estimated speedup: 22-27% (HW-only), 35-45% (aggressive), 50-65% (cascaded with ML)

### What Was Tried

- HW reconstructed frame reference evaluated and rejected (HW artifacts degrade H.266 reference quality)
- Replacing software lookahead evaluated but deferred (RateCtrl TRCPassStats coupling risk)
- H.264 16×16 MB granularity too coarse for 128×128 CTU mapping — 2D aggregation heuristic designed

### What Remains

- Implement all 4 source files in `source/Lib/HWPreAnalysis/`
- Write ffmpeg-based metadata extractor (`gen_hw_metadata.py`)
- Add `VVENC_ENABLE_HW_PREANALYSIS` CMake option and conditional compilation guards
- Add config fields to `vvencCfg.h`
- Wire `HWPreAnalyzer::init()` into `VVEncImpl::init()`
- Insert `HWPreAnalysisStage` before PreProcess in `EncLib::initPass()`
- Implement RateCtrl, GOPCfg, EncCu, InterSearch consumption
- Add `--hw-pre-analysis` and `--hw-metadata` CLI flags
- Create test metadata sidecar files, write unit tests, profile

### Key Technical Details

- Facade: `HWPreAnalyzer` (singleton, vvenc namespace)
- Query interface: `getFrameMetadata(poc, &meta)`, `getFrameComplexity(poc, &float)`, `getSceneCut(poc, &bool)`, `getCUSplitHint(ctuX, ctuY, cuSize, &hint)`, `getMVField(poc, &grid, &w, &h)`
- Heuristic thresholds: MV var < 0.05 + entropy < 0.1 → noSplit (conf 0.9); boundary > 4pel → forceSplit (conf 0.8)
- Sidecar: JSON frontmatter + binary grids (uint8 MB types + int16_t MV pairs per frame)
- Build guard: `VVENC_ENABLE_HW_PREANALYSIS` (CMake option, default OFF)
- Singleton pattern follows `FASTSplitPredictor`
