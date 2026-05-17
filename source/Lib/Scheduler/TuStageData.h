/** \file     TuStageData.h
    \brief    Intermediate pipeline data flowing between TU stages
 */

#pragma once

#include "CommonLib/CommonDef.h"
#include "CommonLib/Unit.h"
#include "CommonLib/Buffer.h"

namespace vvenc {

struct CodingStructure;
class IntraSearch;
class TempCtx;

/// Per-TU+component intermediate data for the 6-stage pipeline.
/// Alloc'd per dispatch. Each stage reads its inputs from this struct
/// and writes its outputs to it. Only FWD_XFORM uses m_pCtxStart (CABAC).
struct TuStageData
{
    // ── Persistent: set once at dispatch init ──
    TempCtx*         m_pCtxStart              = nullptr; ///< CABAC snapshot (FWD_XFORM only)
    IntraSearch*     m_pSearch                = nullptr; ///< IntraSearch instance
    TransformUnit*   m_pTu                    = nullptr; ///< Current TU
    CodingStructure* m_pCs                    = nullptr; ///< TU's CodingStructure
    ComponentID      m_compId                 = COMP_Y;  ///< Current component
    bool             m_checkCrossCPrediction  = false;
    bool             m_loadTr                 = true;
    uint32_t*        m_pOutNumSig             = nullptr; ///< output: numSig for caller
    PelUnitBuf*      m_pOutPredBuf            = nullptr; ///< output: predBuf for caller

    // ── Stage 1 (INIT_PRED): fills m_predBuf from m_pOutPredBuf or intra pred ──
    // ── Stage 2 (RESIDUAL):  m_resiBuf = m_orgBuf - m_predBuf  (luma only)    ──
    // ── Stage 3 (FWD_XFORM): m_absSum, m_coeffs (via transformNxN)            ──
    // ── Stage 4 (INV_XFORM): m_resiBuf = invTransform(m_coeffs)               ──
    // ── Stage 5 (RECON):     m_recoBuf = m_predBuf + m_resiBuf + distortion   ──
    PelBuf           m_orgBuf;
    CPelBuf          m_orgBufC;
    PelBuf           m_predBuf;
    PelBuf           m_resiBuf;
    PelBuf           m_recoBuf;

    // ── Chroma (jointCbCr) ──
    PelBuf           m_crPred;
    PelBuf           m_crResi;
    PelBuf           m_crReco;
    Pel              m_crCoeffs[MAX_TB_SIZEY * MAX_TB_SIZEY];

    // ─── Stage-execution state ──
    bool             m_bSkip                  = false; ///< Early-return from FWD_XFORM
    int              m_absSum                 = 0;     ///< abs(quant coeffs) from transformNxN
    Pel              m_coeffs[MAX_TB_SIZEY * MAX_TB_SIZEY]; ///< copy of coeffs between stages
    int              m_codedCbfMask           = 0;     ///< chroma jointCbCr tracking
    ComponentID      m_codeCompId             = COMP_Y;///< chroma jointCbCr effective comp
    Distortion       m_dist                   = 0;     ///< final SSE distortion
};

}

