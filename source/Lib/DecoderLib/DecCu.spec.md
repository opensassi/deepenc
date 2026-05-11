# DecCu — CU-Level Decoding and Reconstruction

## Overview

`DecCu` handles coding unit (CU) level decoding for the VVenC decoder. It reconstructs intra- and inter-predicted blocks, decodes transform coefficients, and derives motion vectors. The class delegates to `TrQuant`, `IntraPrediction`, and `InterPrediction` instances set during `init()`.

## Class Relationships

```mermaid
graph TB
    DecCu["DecCu"]
    TrQuant["TrQuant<br/>(transform/quantization)"]
    IntraPrediction["IntraPrediction<br/>(spatial prediction)"]
    InterPrediction["InterPrediction<br/>(motion compensation)"]
    CodingUnit["CodingUnit"]
    TransformUnit["TransformUnit"]

    DecCu --> TrQuant
    DecCu --> IntraPrediction
    DecCu --> InterPrediction
    DecCu --> CodingUnit
    DecCu --> TransformUnit

    subgraph Internal Buffers
        m_TmpBuffer["m_TmpBuffer (PelStorage)"]
        m_PredBuffer["m_PredBuffer (PelStorage)"]
        m_triangleMrgCtx["m_triangleMrgCtx (MergeCtx)"]
        m_geoMrgCtx["m_geoMrgCtx (MergeCtx)"]
        m_subPuMiBuf["m_subPuMiBuf (MotionInfo[])"]
    end

    DecCu --- m_TmpBuffer
    DecCu --- m_PredBuffer
    DecCu --- m_triangleMrgCtx
    DecCu --- m_geoMrgCtx
    DecCu --- m_subPuMiBuf
```

## CU Reconstruction Sequence

```mermaid
sequenceDiagram
    participant Caller as DecLib / Caller
    participant Cu as DecCu
    participant Intra as IntraPrediction
    participant Inter as InterPrediction
    participant TQ as TrQuant

    Caller->>Cu: init(pcTrQuant, pcIntra, pcInter, chromaFormat)

    alt Intra CU
        Caller->>Cu: xReconIntraQT(cu)
        Cu->>Cu: xIntraRecQT(cu, chType)
        loop per TransformUnit per component
            Cu->>Cu: xIntraRecBlk(tu, compID)
            Cu->>Intra: predIntra(compID)
            Cu->>TQ: dequant / transform
            Cu->>Cu: reconstruct block
        end
    else Inter CU
        Caller->>Cu: xDeriveCUMV(cu)
        Cu->>Cu: derive merge/AMVP motion vectors
        Caller->>Cu: xReconInter(cu)
        Cu->>Inter: motionCompensation(cu)

        alt non-merge skip
            Caller->>Cu: xDecodeInterTexture(cu)
            loop per TU per component
                Cu->>Cu: xDecodeInterTU(tu, compID)
                Cu->>TQ: residual decoding + dequant
            end
        end
    end
```

## Public Interface

| Method | Description |
|---|---|
| `init(TrQuant*, IntraPrediction*, InterPrediction*, ChromaFormat)` | Set prediction and transform modules |
| `xReconIntraQT(CodingUnit&)` | Reconstruct intra-predicted CU with inverse transform |
| `xReconInter(CodingUnit&)` | Reconstruct inter-predicted CU (MC + residual) |
| `xDecodeInterTexture(CodingUnit&)` | Decode residual coefficients for inter CU |
| `xDeriveCUMV(CodingUnit&)` | Derive motion vectors (merge / AMVP) |
