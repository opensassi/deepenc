# Intra ML Implementation Plan

## ML-based QT-MTT Partitioning for VVC Intra Encoders

Based on: *Tissier et al., "Machine Learning based Efficient QT-MTT Partitioning Scheme for VVC Intra Encoders," IEEE TCSVT, 2022.*

---

## 1. Relationship to Existing ML Module

The existing `MLTools` module implements **Taabane et al. (IEEE Access, 2024)** — 5 binary LightGBM classifiers for **inter** CU split prediction (RA config). This plan extends the same module for **intra** CUs (AI config) following **Tissier et al. (TCSVT, 2022)**.

| Aspect | Existing (Taabane 2024) | Planned (Tissier 2022) |
|--------|-------------------------|------------------------|
| **Target** | Inter CUs (P/B slices) | Intra CUs (I slices) |
| **Method** | 5 binary LGBM classifiers | 16 multi-class LGBM classifiers (one per CU size) |
| **Feature extraction** | Hand-crafted (31 features: texture + motion) | **Option A**: Hand-crafted (~40 intra texture features) |
| | | **Option B**: CNN (226K params) → 480-edge probability vector |
| **Model per** | Split type (QT/BH/BV/TH/TV) | CU pixel size (width-height pairs) |
| **Output classes** | Binary per split (split or no-split) | Multi-class (2–6 classes depending on CU size) |
| **Decision** | Top-N + early skip (thNs=0.25) | Top-N (no early skip) |
| **Results** | 43.21% ETR, 2.9% BDBR (VVenC, RA) | 47.4% ETR, 0.79% BD-BR (VTM, AI) |

---

## 2. Recommended Approach: Option A (Hand-crafted Features)

Skip the CNN — use hand-crafted intra texture features as input to the 16 LGBM models.

**Rationale:**
- No new ML inference dependency (avoid `frugally-deep` or ONNX Runtime)
- Mirrors existing Paper 1 architecture
- Paper ablation (Table VI) shows DT LGBM alone achieves bulk of gain
- VVenC already has aggressive intra fast heuristics — the CNN's spatial features overlap with existing gradient-based tools (`contentBasedFastQtbt`, `qtbttSpeedUp`)
- Hardware constraints: training a 226K-parameter CNN on a laptop is impractical

### 2.1 Feature Vector (~40 features for intra)

Replacing the CNN's 480-element probability vector with hand-crafted features:

| Category | Count | Features |
|----------|-------|----------|
| Texture | 7 | Luma variance, vertical/horizontal gradient, edge strength, DC mean, AC energy, LF ratio |
| Gradient histogram | 16 | 16-bin orientation histogram from Sobel response (weighted by magnitude) |
| CU context | 5 | log2 width, log2 height, QP, QT depth (`cu.depth`), MTT depth (`cu.btDepth`) |
| Neighbor info | 6 | Left/top/top-left CU depths + prediction modes (`cu.intraDir`) |
| Sub-block variance | 4 | 4×4 sub-block variance distribution (mean, min, max, range across sub-blocks) |
| Directional energy | 4 | Energy in horizontal/vertical/diagonal/anti-diagonal high-pass bands (Haar-like) |

Total: **42 features** — all normalised to [0,1] using per-feature max bounds.

### 2.2 The 16 Models (CU Size → Classes)

From Tissier et al. Table IV:

| Width | Height | #classes | Available classes | Notes |
|-------|--------|----------|-------------------|-------|
| 64 | 64 | 2 | QT, NS | Only QT available for 64×64 in VVC |
| 32 | 32 | 6 | QT, BTH, BTV, TTH, TTV, NS | |
| 32 | 16 | 5 | QT, BTH, BTV, TTH, TTV, NS (minus one restricted split) | |
| 16 | 32 | 5 | same | |
| 32 | 8 | 4 | QT restricted or no TT? | Need VVenC geometry check |
| 8 | 32 | 4 | | |
| 32 | 4 | 3 | | |
| 4 | 32 | 3 | | |
| 16 | 16 | 6 | QT, BTH, BTV, TTH, TTV, NS | |
| 16 | 8 | 4 | | |
| 8 | 16 | 4 | | |
| 16 | 4 | 3 | | |
| 4 | 16 | 3 | | |
| 8 | 8 | 3 | BT(1 dir), TT(1 dir), NS | |
| 8 | 4 | 2 | BTH, NS (horizontal BT only) | Per VVC constraints |
| 4 | 8 | 2 | BTV, NS (vertical BT only) | Per VVC constraints |

Class order per model (0-indexed): `{QT, BTH, BTV, TTH, TTV, NS}` with unavailable classes masked to zero.

---

## 3. Module Architecture

### 3.1 New Files

```
source/Lib/MLTools/
├── IntraFeatureExtractor.h         # Intra feature extraction class
├── IntraFeatureExtractor.cpp
├── IntraFeatureExtractor.spec.md
├── IntraSplitPredictor.h           # 16-model multi-class predictor
├── IntraSplitPredictor.cpp
├── IntraSplitPredictor.spec.md
└── MLTools.spec.md                 # UPDATED: add intra module docs

test/
└── vvenc_intramltest/
    ├── vvenc_intramltest.cpp
    ├── vvenc_intramltest.spec.md
    └── CMakeLists.txt
```

### 3.2 Modified Files

```
include/vvenc/vvencCfg.h           # Add mlIntra* config fields
source/Lib/vvenc/vvencCfg.cpp      # Default values + parser
source/Lib/vvenc/vvencimpl.h/.cpp  # Init/release IntraSplitPredictor
source/Lib/EncoderLib/EncCu.cpp    # ML intra split block (for intra slices)
source/Lib/EncoderLib/EncCu.h
source/Lib/EncoderLib/EncModeCtrl.h/.cpp  # m_bMLIntraSkipSplit gate
source/Lib/MLTools/CMakeLists.txt  # Add new source files
source/Lib/EncoderLib/CMakeLists.txt
CMakeLists.txt                     # Feature toggle
```

### 3.3 Class Declarations

**IntraFeatureExtractor:**

```cpp
#pragma once
#include <vector>

namespace vvenc {

class CodingUnit;
class Partitioner;

class IntraFeatureExtractor {
public:
    IntraFeatureExtractor();
    virtual ~IntraFeatureExtractor();

    /** \brief Extract intra feature vector from CU context
     *  \param[in]  cu           Current intra CU
     *  \param[in]  partitioner  Active partitioner
     *  \param[out] outFeatures  Feature vector (~42 elements)
     *  \retval 0  Success
     *  \retval -1 Feature extraction failed (invalid CU state)
     */
    int extract(const CodingUnit& cu,
                const Partitioner& partitioner,
                std::vector<double>& outFeatures);

private:
    int xAddTextureFeatures(const CodingUnit& cu);
    int xAddGradientHistogram(const CodingUnit& cu);
    int xAddContextFeatures(const CodingUnit& cu, const Partitioner& partitioner);
    int xAddNeighborFeatures(const CodingUnit& cu);
    int xAddSubBlockVariance(const CodingUnit& cu);
    int xAddDirectionalEnergy(const CodingUnit& cu);
    void xReset();

    std::vector<double> m_vFeatures;
};

}
```

**IntraSplitPredictor:**

```cpp
#pragma once
#include <string>
#include <vector>
#include <utility>

namespace vvenc {

class IntraSplitPredictor {
public:
    enum SplitType : int {
        NS       = 0,
        QT_SPLIT = 1,
        BH_SPLIT = 2,
        BV_SPLIT = 3,
        TH_SPLIT = 4,
        TV_SPLIT = 5,
    };

    static constexpr int NUM_CU_SIZES = 16;

    IntraSplitPredictor();
    virtual ~IntraSplitPredictor();

    static IntraSplitPredictor* getInstance();
    static void setInstance(IntraSplitPredictor* predictor);

    /** \brief Load 16 multi-class LightGBM models from model directory
     *  Model files named: intra_<w>x<h>.txt (e.g., intra_64x64.txt, intra_32x32.txt)
     *  \param[in] modelDir  Path to directory containing model files
     *  \retval 0  Success
     *  \retval -1 Model file not found or load failed
     */
    int init(const std::string& modelDir);

    /** \brief Predict top-N split candidates for a CU of given size
     *  \param[in]  features      Feature vector
     *  \param[in]  cuWidth      CU width
     *  \param[in]  cuHeight     CU height
     *  \param[in]  nCandidates  Max candidates to return (top-N, default 3)
     *  \param[in]  allowedSplits Bitmask of canSplit-valid modes
     *  \param[out] outCandidates Ranked list of (SplitType, confidence) pairs
     *  \retval 0  Success
     *  \retval -1 Not initialised or unsupported CU size
     */
    int predict(const std::vector<double>& features,
                int cuWidth,
                int cuHeight,
                int nCandidates,
                unsigned int allowedSplits,
                std::vector<std::pair<SplitType, double>>& outCandidates);

    int release();
    bool isInitialized() const;

    /** \brief Map CU dimensions to model index */
    static int cuSizeToModelIndex(int width, int height);

    static PartSplit splitTypeToPartSplit(SplitType type);

private:
    int xLoadBooster(const std::string& path, void** handle);
    int xPredictOne(void* handle, const std::vector<double>& features,
                    std::vector<double>& outProbs);

    /** \brief CU size descriptor for model lookup */
    struct CuSizeDesc {
        int width;
        int height;
        int numClasses;  // 2..6
    };

    static const CuSizeDesc m_supportedSizes[NUM_CU_SIZES];

    void*         m_hBoosters[NUM_CU_SIZES];  // LightGBM booster handles
    bool          m_bInitialized;
    std::string   m_cModelDir;
};

}
```

---

## 4. Config Fields

### 4.1 vvenc_config additions

```c
// Intra ML (Tissier 2022)
int   m_mlIntraEnable;                    // 0=off, 1=on (default: 0)
int   m_mlIntraTopK;                      // top-N candidates (default: 3)
double m_mlIntraCn;                       // confidence threshold (default: 0.3)
char  m_mlIntraModelDir[VVENC_MAX_STRING_LEN];  // path to 16 model files
```

### 4.2 CLI parameters

| CLI flag | Config field | Default |
|----------|-------------|---------|
| `--ml-intra-enable` | `m_mlIntraEnable` | 0 |
| `--ml-intra-topk` | `m_mlIntraTopK` | 3 |
| `--ml-intra-cn` | `m_mlIntraCn` | 0.3 |
| `--ml-intra-model-dir` | `m_mlIntraModelDir` | "" |

### 4.3 Feature toggle

```cmake
option(VVENC_ENABLE_ML_INTRA_LIGHTGBM "Enable ML-guided intra CU split prediction" OFF)
```

---

## 5. Integration Points in EncCu

### 5.1 Main call site: `EncCu::xCompressCU()`

Inserted after non-split mode testing and before the split search loop (parallel to the existing inter ML block at lines 1058–1138), but only active for intra slices:

```cpp
// Inside xCompressCU(), after non-split modes, before split loop
#if VVENC_ENABLE_ML_INTRA_LIGHTGBM
if (slice.isIntra() && m_pcEncCfg->m_mlIntraEnable && !partitioner.isConsInter())
{
    IntraSplitPredictor* predictor = IntraSplitPredictor::getInstance();
    if (predictor && predictor->isInitialized())
    {
        IntraFeatureExtractor extractor;
        std::vector<double> features;
        const CodingUnit* mlCu = tempCS->getCU(partitioner.chType, partitioner.treeType);
        int ret = (mlCu) ? extractor.extract(*mlCu, partitioner, features) : -1;

        if (ret == 0 && features.size() > 0)
        {
            unsigned int allowedMask = xBuildIntraAllowedMask(partitioner, *tempCS);
            std::vector<std::pair<IntraSplitPredictor::SplitType, double>> candidates;
            ret = predictor->predict(features, cu.lumaSize().width, cu.lumaSize().height,
                                     m_pcEncCfg->m_mlIntraTopK, allowedMask, candidates);

            if (ret == 0 && !candidates.empty())
            {
                m_modeCtrl.setMLIntraSkipSplit(true);
                for (const auto& cand : candidates)
                {
                    PartSplit ps = IntraSplitPredictor::splitTypeToPartSplit(cand.first);
                    if (!partitioner.canSplit(ps, *tempCS)) continue;
                    EncTestMode mlMode = xMapSplitToEncTestMode(ps, qp, lossless);
                    xCheckModeSplit(tempCS, bestCS, partitioner, mlMode);
                }
                if (bestCS->cost < MAX_DOUBLE)
                    return;  // early return, skip exhaustive split search
            }
        }
    }
}
#endif
```

### 5.2 Mode control gating

In `EncModeCtrl`:
```cpp
bool m_bMLIntraSkipSplit = false;  // set by EncCu before split loop
void setMLIntraSkipSplit(bool v) { m_bMLIntraSkipSplit = v; }

// In trySplit():
if (m_bMLIntraSkipSplit || m_bMLSkipSplit) return false;
```

---

## 6. Training Data Generation

### 6.1 CSV format

Activated by `VVENC_ML_INTRA_TRAINING_OUT` env var:

```
poc,ctu_x,ctu_y,cu_x,cu_y,cu_w,cu_h,
feat_0,...,feat_41,
label_qt,label_bth,label_btv,label_tth,label_ttv,label_ns
```

Labels are one-hot: `1` for the ground-truth optimal split chosen by exhaustive RDO.

### 6.2 Training pipeline (Python, outside VVenC)

```
1. Encode CTC sequences (AI config, QPs 22/27/32/37) with VVENC_ML_INTRA_TRAINING_OUT
2. Split CSV by CU size into 16 sub-datasets
3. For each CU size sub-dataset:
   a. Balance classes (undersample majority)
   b. Split 70/15/15 train/val/test
   c. Train LightGBM multi-class classifier with cross-entropy loss
   d. Hyperparameter tuning via Optuna (TPE sampler)
   e. Export model to native .txt format
4. Place 16 .txt files in ml-intra-models/
```

### 6.3 Training hyperparameters (from Tissier et al. Table VI)

| Parameter | Value |
|-----------|-------|
| `num_leaves` | 31 |
| `learning_rate` | 0.08 |
| `feature_fraction` | 0.9 |
| `bagging_fraction` | 0.8 |
| `bagging_freq` | 1 |
| `min_data_in_leaf` | 20 |
| `lambda_l1` | 0.1 |
| `lambda_l2` | 0.1 |
| `objective` | `multiclass` |
| `num_class` | 2–6 (per model) |

---

## 7. Testing Requirements

### 7.1 Unit tests (`test/vvenc_intramltest/vvenc_intramltest.cpp`)

| Test ID | Scope | What to Verify |
|---------|-------|---------------|
| `INTRA_PREDICTOR_LOAD` | IntraSplitPredictor | `init()` with valid model dir returns 0, 16 models loaded |
| `INTRA_PREDICTOR_BAD_DIR` | IntraSplitPredictor | `init()` with missing dir returns error |
| `INTRA_PREDICTOR_MODEL_COUNT` | IntraSplitPredictor | All 16 booster handles non-null after init |
| `INTRA_PREDICTOR_SIZE_SELECT` | IntraSplitPredictor | `cuSizeToModelIndex(32,32)` → correct model index |
| `INTRA_PREDICTOR_TOP3_64x64` | IntraSplitPredictor | `predict()` for 64×64 returns at most 2 candidates (only QT + NS) |
| `INTRA_PREDICTOR_TOP3_32x32` | IntraSplitPredictor | `predict()` for 32×32 returns at most 3 of 6 classes |
| `INTRA_PREDICTOR_UNSUPPORTED` | IntraSplitPredictor | `predict()` for unsupported size (e.g., 4×4) returns error |
| `INTRA_PREDICTOR_INIT_TWICE` | IntraSplitPredictor | Double init returns error |
| `INTRA_PREDICTOR_PREDICT_BEFORE_INIT` | IntraSplitPredictor | predict before init returns error |
| `INTRA_PREDICTOR_RELEASE` | IntraSplitPredictor | Release frees all 16 boosters, subsequent predict fails |
| `INTRA_FEATURE_SIZE` | IntraFeatureExtractor | `extract()` produces ~42-element vector |
| `INTRA_FEATURE_VALUES` | IntraFeatureExtractor | Features in [0,1] range, known inputs produce expected outputs |
| `INTRA_FEATURE_GRAD_HIST` | IntraFeatureExtractor | Gradient histogram for uniform block: one bin dominates |
| `INTRA_FEATURE_FLAT_BLOCK` | IntraFeatureExtractor | Flat block → near-zero variance, energy features |
| `INTRA_FEATURE_EMPTY_CU` | IntraFeatureExtractor | Invalid/empty CU returns error |

### 7.2 Calling-order validation

| Test | What to Verify |
|------|---------------|
| `init()` → `predict()` → `release()` | Valid lifecycle |
| `predict()` before `init()` | Returns error |
| `release()` twice | No crash, returns error |
| `init()` after `release()` | Can re-initialise |

### 7.3 Integration tests

| Test | What to Verify |
|------|---------------|
| Full intra-only encode with dummy models (constant 0.5) | 16 frames encode without crash |
| Full encode with `--ml-intra-cn 1.0` | All predictions rejected, behaves identical to unmodified encode |
| `VVENC_ENABLE_ML_INTRA_LIGHTGBM=OFF` | Binary identical to unmodified VVenC |
| Training CSV output (`VVENC_ML_INTRA_TRAINING_OUT`) | Valid CSV, correct feature count per CU |
| All 16 model directories enumerated | Graceful handling of partial model sets |

### 7.4 Performance benchmarks

| Benchmark | Metric |
|-----------|--------|
| Intra-only encode at CTC classes A1–E (QPs 22/27/32/37) | ∆ET (encoding time reduction) vs VVenC anchor |
| BD-BR calculation per class | BD-BR loss relative to anchor |
| Inference overhead per frame | % of encoding time spent in ML prediction |
| Top-3 vs Top-2 tradeoff | Compare complexity reduction vs BD-BR |

Benchmarks require a CTC dataset and are **outside laptop scope** — document the procedure for future server-based evaluation.

---

## 8. Results Target

Based on Tissier et al. (target: VTM 10.2), with expected VVenC delta:

| Configuration | VTM result (paper) | Expected VVenC target |
|---------------|-------------------|----------------------|
| Top-3 | 47.4% ∆ET, 0.79% BD-BR | ~30–40% ∆ET, ~1.0–1.5% BD-BR |
| Top-2 | 70.4% ∆ET, 2.49% BD-BR | ~50–60% ∆ET, ~2.5–3.5% BD-BR |

Note: VVenC already has aggressive intra fast tools (`qtbttSpeedUp`, `contentBasedFastQtbt`, etc.) that the VTM anchor does not have. The ML module will be additive to VVenC's existing fast intra heuristics, so absolute gains will be lower than the paper's VTM-to-VTM comparison.

---

## 9. Future Enhancements (Post-laptop)

- **CNN integration**: Add `IntraCNNFeatureExtractor` using pre-trained ONNX model. Replace hand-crafted features with 480-element edge-probability vector. Requires `onnxruntime` C++ dependency.
- **Weighted fallback**: Blend ML predictions with VVenC's existing `contentBasedFastQtbt` gradient-based split cost heuristic.
- **Per-frame retraining**: Online adaptation of LGBM models using confirmed splits from preceding frames.
- **Extension to RA**: Combine inter ML (Taabane) with intra ML (Tissier) — use intra model for I-frames, inter model for P/B-frames.
