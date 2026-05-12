# EncCu — Coding Unit Encoder

## 1) Purpose

`EncCu` is the core CU-level encoding engine. It performs mode decision (intra, inter, merge/skip, IBC), split decision (QT/BT/TT), and manages the full CU encode pipeline for a single coding unit.

## 2) Class Diagram

```mermaid
graph TB
    EncCu --> DecCu
    EncCu --> IntraSearch
    EncCu --> InterSearch
    EncCu --> EncModeCtrl
    EncCu --> TrQuant
    EncCu --> LoopFilter
    EncCu --> RdCost
    EncCu --> MergeItemList
    EncCu --> FastGeoCostList
    EncCu --> GeoComboCostList
    EncCu --> MergeItem
```

## 3) Key Methods

| Method | Description |
|---|---|
| `init()` | Initialize with encoding config, SPS, QP vector, and rate control |
| `encodeCtu()` | Encode a single CTU at given tile-grid position |
| `xCompressCtu()` | Top-level CTU compression dispatcher |
| `xCompressCU()` | Recursive CU mode decision (split vs. no-split) |
| `xCheckRDCostIntra()` | RD-cost check for intra prediction modes |
| `xCheckRDCostInter()` | RD-cost check for inter prediction modes |
| `xCheckRDCostUnifiedMerge()` | Unified merge/skip mode RD-cost check |
| `xCheckRDCostIBCMode()` | Intra-block copy mode RD-cost check |
| `xCheckModeSplit()` | Evaluate whether to split current CU |

## 4) Dependencies

- **Inherits**: `DecCu` (decoder CU helper)
- **Owns**: `IntraSearch`, `InterSearch`, `EncModeCtrl`, `TrQuant`, `RdCost`, `LoopFilter`
- **Uses**: `CABACWriter`, `RateCtrl`, `Picture`, `CodingStructure`, `UnitPartitioner`
- **Optional**: `CUFeatureExtractor`, `FASTSplitPredictor` (from `MLTools`, conditional on `VVENC_ENABLE_ML_LIGHTGBM`)
- **Config**: `mlEnable`, `mlThNs` (no-skip threshold, default 0.25), `mlTopK` (candidates, default 3), `mlModelDir`

## 5) Data Flow

```mermaid
sequenceDiagram
    participant EncSlice
    participant EncCu
    participant IntraSearch
    participant InterSearch
    participant EncModeCtrl
    participant CUFeatureExtractor
    participant FASTSplitPredictor
    participant Partitioner

    EncSlice->>EncCu: encodeCtu(pic, prevQP, ctuX, ctuY)
    EncCu->>EncCu: xCompressCtu()
    loop per depth/partition
        EncCu->>EncModeCtrl: tryMode(testMode)
        alt Intra Mode
            EncCu->>IntraSearch: predIntra(comp, mode)
            IntraSearch-->>EncCu: prediction buffer
        else Inter Mode
            EncCu->>InterSearch: motionEstimation(cu)
            InterSearch-->>EncCu: MV + cost
        end
        EncCu->>EncCu: xCheckBestMode()
    end
    opt ML dual-path enabled (Taabane 2024 Algorithm 1)
        EncCu->>CUFeatureExtractor: extract(cu, partitioner)
        CUFeatureExtractor-->>EncCu: ~30-element feature vector
        EncCu->>Partitioner: build allowedSplits bitmask
        Partitioner-->>EncCu: allowed splits for current geometry
        EncCu->>FASTSplitPredictor: predict(features, topK=3, thNs=0.25, allowedSplits)
        alt bEarlySkip == true (max confidence < 0.25)
            FASTSplitPredictor-->>EncCu: early skip signal
            Note over EncCu: encode CU without splitting, skip RDO
        else top-N candidates returned
            FASTSplitPredictor-->>EncCu: [(split1, conf1), (split2, conf2), (split3, conf3)]
            loop for each candidate (max 3)
                EncCu->>Partitioner: canSplit(candidate, tempCS)
                alt canSplit == true
                    Partitioner-->>EncCu: valid
                    EncCu->>EncCu: xCheckModeSplit(candidate, tempCS, bestCS)
                else invalid for geometry
                    Partitioner-->>EncCu: skip
                end
            end
            Note over EncCu: RDO picks best among tested candidates
            alt bestCS->cost < MAX_DOUBLE
                EncCu->>EncCu: return (skip un-tested splits)
            else all candidates worse than no-split
                Note over EncCu: fall through to full RDO search
            end
        end
    end
    EncCu-->>EncSlice: encoded CTU data
```

## 6) Configuration

| Field | Source | Effect |
|---|---|---|---|
| `VVEncCfg::m_QP` | Encoder config | Base quantization parameter |
| `VVEncCfg::m_IntraPeriod` | Encoder config | Intra frame refresh period |
| `VVEncCfg::m_MaxMergeNum` | Encoder config | Maximum merge candidates |
| `VVEncCfg::m_BiPred` | Encoder config | Enable bi-predictive inter |
| `VVEncCfg::m_mlEnable` | Encoder config | Enable ML-guided CU split prediction (0/1) |
| `VVEncCfg::m_mlThNs` | Encoder config | No-skip confidence threshold (default 0.25) |
| `VVEncCfg::m_mlTopK` | Encoder config | Top-K candidates to evaluate (default 3) |
| `VVEncCfg::m_mlModelDir` | Encoder config | Path to LightGBM model files |
| `SPS::maxCuDepth` | Sequence params | Max CU split depth |

## 7) Lifecycle

```
init() → [initPic() / initSlice() per picture/slice]
  → setUpLambda() for each slice
    → encodeCtu() for each CTU in raster order
      → xCompressCtu() → xCompressCU() recursive split
destroy()
```
