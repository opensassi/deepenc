/** \file     WorkUnit.h
    \brief    Dispatchable pipeline stage unit (standalone, no encoder deps)
 */

#pragma once

#include <cstdint>
#include <atomic>

namespace vvenc {

static constexpr uint8_t SCHED_COMP_Y  = 0;
static constexpr uint8_t SCHED_COMP_Cb = 1;
static constexpr uint8_t SCHED_COMP_Cr = 2;

enum class Stage : uint8_t
{
    INIT_PRED    = 0,
    PREDICT      = 1,
    RESIDUAL     = 2,
    FWD_XFORM    = 3,
    LFNST_FWD    = 4,
    QUANT_FILL   = 5,
    QUANT_TRACE  = 6,
    DEQUANT      = 7,
    LFNST_INV    = 8,
    INV_XFORM    = 9,
    RECONSTRUCT  = 10,
    DISTORTION   = 11,

    CTU_ENCODE   = 12,
    RECON_WRITE  = 13,
    LF_VER       = 14,
    LF_HOR       = 15,
    SAO_FILTER   = 16,
    ALF_STATS    = 17,
    ALF_DERIVE   = 18,
    ALF_RECON    = 19,
    CCALF_STATS  = 20,
    CCALF_DERIVE = 21,
    CCALF_RECON  = 22,

    _COUNT       = 23
};

static constexpr uint8_t SPATIAL_LEFT      = 1 << 0;
static constexpr uint8_t SPATIAL_TOP       = 1 << 1;
static constexpr uint8_t SPATIAL_TOP_RIGHT = 1 << 2;
static constexpr uint8_t SPATIAL_BOT_RIGHT = 1 << 3;
static constexpr uint8_t SPATIAL_RIGHT     = 1 << 4;
static constexpr uint8_t SPATIAL_BOTTOM    = 1 << 5;

struct WorkUnit;

using WorkFunc = bool (*)(WorkUnit* pWu, void* pScratch);

struct WorkUnit
{
    Stage           m_eStage;

    uint32_t        m_tuId;

    uint8_t         m_compId;

    int             m_width     = 0;
    int             m_height    = 0;

    int8_t          m_qp        = 0;

    uint8_t         m_mtsIdx    = 0;

    bool            m_bCbf      = false;

    uint32_t        m_ctuRsAddr    = 0;

    uint16_t        m_ctuPosX      = 0;

    uint16_t        m_ctuPosY      = 0;

    uint8_t         m_spatialDepMask = 0;

    std::atomic<int> m_depCount{ 0 };

    static constexpr int MAX_DEPS = 4;

    WorkUnit*       m_pDependents[MAX_DEPS] = {};

    int             m_numDependents  = 0;

    void*           m_pInputBuf      = nullptr;

    void*           m_pOutputBuf     = nullptr;

    void*           m_pCtx           = nullptr;

    void*           m_pScratch       = nullptr;

    WorkFunc        m_pfnExec        = nullptr;
};

}
