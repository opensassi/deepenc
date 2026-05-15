# WorkUnit — Dispatchable Pipeline Stage Unit

## 1. Overview

`WorkUnit` is the fundamental dispatchable unit in the scheduler. It represents one pipeline stage for one component of one TU. The `Stage` enum defines every possible operation in the TU pipeline. The `WorkFunc` typedef is the function pointer signature for executing a stage.

**Dependencies**: `std::atomic`, `<cstdint>`, `CommonDef.h` (for `ComponentID`). No encoder-specific headers.

**Lifecycle**: WorkUnits are created by `TUPipelineDAG::build()` (per-mode-trial) or `PictureDAG::build()` (per-frame wavefront) in a pre-allocated pool. They are not individually allocated or freed — the pool is reused across frames.

## 2. Component Specifications

### 2.1 Enum: `Stage`

```cpp
#pragma once

#include <cstdint>
#include <atomic>

namespace vvenc {

enum class Stage : uint8_t
{
    // —— TU pipeline stages (temporal: one mode trial of one CU) ——
    INIT_PRED    = 0,   ///< Prepare intra reference samples
    PREDICT      = 1,   ///< Generate prediction signal (intra or inter)
    RESIDUAL     = 2,   ///< resi = org - pred
    FWD_XFORM    = 3,   ///< 2D separable DCT/DST forward transform
    LFNST_FWD    = 4,   ///< LFNST reduction (optional, gated by TU size)
    QUANT_FILL   = 5,   ///< RDOQ cost array fill + DepQuant trellis forward pass
    QUANT_TRACE  = 6,   ///< DepQuant backward trace + level extraction
    DEQUANT      = 7,   ///< Inverse quantization
    LFNST_INV    = 8,   ///< LFNST inverse (optional)
    INV_XFORM    = 9,   ///< 2D inverse DCT/DST
    RECONSTRUCT  = 10,  ///< reco = pred + resi
    DISTORTION   = 11,  ///< SSE computation

    // —— CTU pipeline stages (spatial: wavefront across CTUs in a frame) ——
    CTU_ENCODE   = 12,  ///< Full mode decision + TU pipeline for one CTU
    RECON_WRITE  = 13,  ///< Write final CTU reconstruction to picture buffer
    LF_VER       = 14,  ///< Vertical deblocking filter
    LF_HOR       = 15,  ///< Horizontal deblocking filter
    SAO_FILTER   = 16,  ///< Sample adaptive offset
    ALF_STATS    = 17,  ///< ALF gradient statistics collection
    ALF_DERIVE   = 18,  ///< ALF filter derivation
    ALF_RECON    = 19,  ///< ALF reconstruction
    CCALF_STATS  = 20,  ///< CCALF statistics
    CCALF_DERIVE = 21,  ///< CCALF filter derivation
    CCALF_RECON  = 22,  ///< CCALF reconstruction

    _COUNT       = 23
};

}
```

### 2.2 Constants: Spatial Dependency Types

```cpp
namespace vvenc {

/// Bitmask flags for m_spatialDepMask on CTU-level WorkUnits.
/// A non-zero mask means the unit must check neighbor-completion
/// before it becomes dispatchable.
static constexpr uint8_t SPATIAL_LEFT      = 1 << 0;
static constexpr uint8_t SPATIAL_TOP       = 1 << 1;
static constexpr uint8_t SPATIAL_TOP_RIGHT = 1 << 2;
static constexpr uint8_t SPATIAL_BOT_RIGHT = 1 << 3;

}
```

TU-internal WorkUnits set `m_spatialDepMask = 0`. CTU-level stages (CTU_ENCODE, LF_VER, etc.) set the appropriate neighbor bits. The scheduler checks `m_pCtuStates[neighborAddr] >= requiredStage` before dispatching.

### 2.3 Typedef: `WorkFunc`

```cpp
namespace vvenc {

/** \brief Work unit executor function pointer.
 *  \param[in,out] pWu      the work unit to execute
 *  \param[in,out] pScratch per-thread scratch buffer pointer
 *  \return true if execution succeeded (unit completes), false to reschedule
 */
using WorkFunc = bool (*)(WorkUnit* pWu, void* pScratch);

}
```

### 2.4 Struct: `WorkUnit`

```cpp
namespace vvenc {

struct WorkUnit
{
    /// Pipeline stage this unit represents (TU-level or CTU-level)
    Stage           m_eStage;

    /// Flat TU or CU index within the containing DAG
    uint32_t        m_tuId;

    /// Colour component index (COMP_Y, COMP_Cb, COMP_Cr)
    ComponentID     m_compId;

    /// Transform block dimensions (TU-level only)
    int             m_width     = 0;
    int             m_height    = 0;

    /// Quantization parameter
    int8_t          m_qp        = 0;

    /// MTS index for transform selection
    uint8_t         m_mtsIdx    = 0;

    /// Coded block flag (pre-computed by earlier stage)
    bool            m_bCbf      = false;

    /// --- CTU spatial metadata (for wavefront dispatch) ---

    /// CTU raster-scan address in the picture (0..numCtuInPic-1)
    uint32_t        m_ctuRsAddr    = 0;

    /// CTU grid column
    uint16_t        m_ctuPosX      = 0;

    /// CTU grid row
    uint16_t        m_ctuPosY      = 0;

    /// Spatial dependency type mask (0 for TU-internal units).
    /// Bits: SPATIAL_LEFT, SPATIAL_TOP, SPATIAL_TOP_RIGHT, SPATIAL_BOT_RIGHT.
    uint8_t         m_spatialDepMask = 0;

    /// --- Dependency tracking ---

    /// Number of remaining dependencies. When 0 and spatial deps met, ready.
    std::atomic<int> m_depCount{ 0 };

    /// List of dependent work units; each gets depCount-- on completion.
    WorkUnit**      m_pDependents    = nullptr;

    /// Number of entries in m_pDependents
    int             m_numDependents  = 0;

    /// --- Buffer I/O ---

    /// Input buffer pointer (from previous stage's output or ring buffer)
    void*           m_pInputBuf      = nullptr;

    /// Output buffer pointer (to next stage's input)
    void*           m_pOutputBuf     = nullptr;

    /// Scratch buffer for this unit's execution (trellis, cost arrays)
    void*           m_pScratch       = nullptr;

    /// Executor function for this stage
    WorkFunc        m_pfnExec        = nullptr;
};

}
```

## 3. System Architecture

```mermaid
graph TB
    subgraph WorkUnit["WorkUnit Struct"]
        Stage["m_eStage<br/>INIT_PRED..CCALF_RECON"]
        Spatial["m_ctuRsAddr / m_ctuPosX/Y<br/>m_spatialDepMask"]
        Deps["m_depCount / m_pDependents<br/>dependency tracking"]
        Buf["m_pInputBuf / m_pOutputBuf<br/>ring buffer slots"]
        Exec["m_pfnExec<br/>function pointer"]
        Meta["m_tuId / m_compId<br/>m_width / m_height / m_qp"]
    end

    subgraph Pool["Pre-allocated Pool"]
        WU0["WorkUnit [0]"]
        WU1["WorkUnit [1]"]
        WUN["WorkUnit [N]"]
    end

    subgraph DAG_Builders["DAG Builders"]
        tDAG["TUPipelineDAG<br/>per-mode-trial"]
        pDAG["PictureDAG<br/>per-frame wavefront"]
    end

    subgraph Executor["Executor Functions"]
        predFn["xIntraPredAng<br/>xMotionComp"]
        residFn["sub pel<br/>resi = org - pred"]
        xformFn["xT<br/>2D separable DCT"]
        quantFn["DepQuant::xQuantDQ"]
        invFn["xIT / xDeQuant"]
        reconFn["reco = pred + resi"]
        ctuFn["xProcessCtuTask<br/>wavefront state machine"]
    end

    Pool -->|filled by| tDAG
    Pool -->|filled by| pDAG
    tDAG -->|creates edges| Deps
    pDAG -->|creates spatial edges| Deps
    pDAG -->|sets| Spatial
    Stage -->|selects| Exec
    Exec -->|dispatches to| predFn
    Exec -->|dispatches to| residFn
    Exec -->|dispatches to| xformFn
    Exec -->|dispatches to| quantFn
    Exec -->|dispatches to| invFn
    Exec -->|dispatches to| reconFn
    Exec -->|dispatches to| ctuFn
    Buf -->|reads/writes| RingBuffer
```

## 4. Detailed Data Flow

No sequence diagram — the WorkUnit is a passive data structure, not an active participant. Its flow is described by the `TUPipelineDAG` sequence diagram, which creates and links WorkUnits, and the `TUScheduler` sequence diagram, which dispatches them.

## 5. Visualization

No D3 animation — a struct definition has no temporal state machine to verify.

## 6. Testing Requirements

### Unit Tests

| Test | What to Verify |
|------|----------------|
| Stage enum uniqueness | All values 0..22, no duplicates, _COUNT = 23 |
| SpatialDepType constants | SPATIAL_LEFT=1, SPATIAL_TOP=2, TOP_RIGHT=4, BOT_RIGHT=8, no overlap |
| CTU-level unit defaults | ctuRsAddr=0, ctuPosX=0, ctuPosY=0, spatialDepMask=0 |
| TU-level spatial mask | TU WorkUnits set spatialDepMask=0 by default |
| WorkUnit default state | depCount=0, all pointers null |
| depCount decrement | CAS correctly produces 0 after N decrements |
| depCount negative guard | Cannot decrement below 0 (unsigned/saturating) |
| WorkFunc signature | Can be constructed from `bool (*)(WorkUnit*, void*)` |
