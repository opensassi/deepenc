/** \file     SchedulerExecutors.cpp
    \brief    Per-stage executor functions for the 5-stage TU pipeline.
             Each executor calls exactly one sub-step of xIntraCodingTUBlock.
             Only FWD_XFORM restores CABAC (needed for RDOQ).
             Early returns propagate via ctx->m_bSkip.
 */

#include "SchedulerExecutors.h"
#include "TuStageData.h"
#include "WorkUnit.h"

#include "EncoderLib/IntraSearch.h"
#include "EncoderLib/InterSearch.h"
#include "CommonLib/CodingStructure.h"
#include "CommonLib/Contexts.h"
#include "CommonLib/Unit.h"
#include "CommonLib/UnitTools.h"
#include "CommonLib/TypeDef.h"

namespace vvenc {

extern bool g_schedulerActive;

// ═══════════════════════════════════════════════════════════════════
// Stage 1: INIT_PRED (+ PREDICT)
// ═══════════════════════════════════════════════════════════════════
// Initializes intra reference pattern and generates prediction signal.
// Handles: ISP, MIP, predBuf-copy, Angular (luma + chroma).
// No CABAC dependency. Writes m_predBuf.
bool SchedulerExecutors::execInitPred(WorkUnit* pWu, void* pScratch)
{
    (void)pScratch;
    if (!pWu || !pWu->m_pCtx) return false;

    TuStageData* ctx = (TuStageData*)pWu->m_pCtx;
    if (!ctx->m_pSearch || !ctx->m_pTu) return false;

    IntraSearch* pSearch = ctx->m_pSearch;
    TransformUnit& tu = *ctx->m_pTu;
    const ComponentID compID = ctx->m_compId;
    CodingStructure& cs = *ctx->m_pCs;
    const CompArea& area = tu.blocks[compID];

    // ── Caller-provided predBuf: copy from it instead of computing ──
    if (ctx->m_pOutPredBuf && isLuma(compID))
    {
        ctx->m_predBuf.copyFrom(ctx->m_pOutPredBuf->Y());
        return true;
    }

    // ── Luma ──
    if (isLuma(compID))
    {
        bool predRegDiffFromTB = CU::isPredRegDiffFromTB(*tu.cu);
        bool firstTBInPredReg  = false;
        CompArea areaPredReg(COMP_Y, tu.chromaFormat, area);

        if (tu.cu->ispMode)
        {
            firstTBInPredReg = CU::isFirstTBInPredReg(*tu.cu, area);
            if (predRegDiffFromTB)
            {
                if (firstTBInPredReg)
                {
                    CU::adjustPredArea(areaPredReg);
                    PelBuf recoForIsp = cs.getRecoBuf(areaPredReg);
                    pSearch->initIntraPatternChTypeISP(*tu.cu, areaPredReg, recoForIsp);
                }
            }
            else
            {
                PelBuf recoForIsp = cs.getRecoBuf(area);
                pSearch->initIntraPatternChTypeISP(*tu.cu, area, recoForIsp);
            }
        }
        else
        {
            pSearch->initIntraPatternChType(*tu.cu, area);
        }

        // Generate prediction signal
        if (predRegDiffFromTB)
        {
            if (firstTBInPredReg)
            {
                PelBuf piPredReg = cs.getPredBuf(areaPredReg);
                pSearch->predIntraAng(compID, piPredReg, *tu.cu);
            }
        }
        else
        {
            if (CU::isMIP(*tu.cu, CH_L))
            {
                pSearch->initIntraMip(*tu.cu);
                pSearch->predIntraMip(ctx->m_predBuf, *tu.cu);
            }
            else
            {
                pSearch->predIntraAng(compID, ctx->m_predBuf, *tu.cu);
            }
        }
    }
    // ── Chroma ──
    else
    {
        pSearch->initIntraPatternChType(*tu.cu, area);
        pSearch->predIntraAng(compID, ctx->m_predBuf, *tu.cu);
    }

    return true;
}

// ═══════════════════════════════════════════════════════════════════
// Stage 2: RESIDUAL (luma only)
// ═══════════════════════════════════════════════════════════════════
// resi = org - pred. Handles LMCS reshaping variant.
// No CABAC dependency. Writes m_resiBuf.
bool SchedulerExecutors::execResidual(WorkUnit* pWu, void* pScratch)
{
    (void)pScratch;
    if (!pWu || !pWu->m_pCtx) return false;

    TuStageData* ctx = (TuStageData*)pWu->m_pCtx;
    if (!ctx->m_pTu) return false;

    TransformUnit& tu = *ctx->m_pTu;
    CodingStructure& cs = *ctx->m_pCs;

    // Residual is only computed for luma here. Chroma is handled inside FWD_XFORM.
    if (!isLuma(ctx->m_compId)) return true;

    const Slice& slice = *cs.slice;
    bool lmcsFlag = cs.picHeader->lmcsEnabled && (slice.isIntra() || (!slice.isIntra() && cs.picture->reshapeData.getCTUFlag()));

    if (cs.picHeader->lmcsEnabled && cs.picture->reshapeData.getCTUFlag())
    {
        ctx->m_resiBuf.subtract(cs.getRspOrgBuf(tu.blocks[ctx->m_compId]), ctx->m_predBuf);
    }
    else
    {
        ctx->m_resiBuf.subtract(ctx->m_orgBuf, ctx->m_predBuf);
    }

    return true;
}

// ═══════════════════════════════════════════════════════════════════
// Stage 3: FWD_XFORM
// ═══════════════════════════════════════════════════════════════════
// Forward transform + quantization. The ONLY CABAC-dependent stage.
// Restores CABAC from per-TU snapshot. Handles luma + chroma (incl. jointCbCr).
// Sets m_absSum, m_coeffs, m_bSkip, m_codeCompId, m_codedCbfMask.
// MAY RETURN EARLY via m_bSkip=true for ISP all-zero or jointCbCr mismatch.
bool SchedulerExecutors::execFwdXform(WorkUnit* pWu, void* pScratch)
{
    (void)pScratch;
    if (!pWu || !pWu->m_pCtx) return false;

    TuStageData* ctx = (TuStageData*)pWu->m_pCtx;
    if (!ctx->m_pSearch || !ctx->m_pTu) return false;

    IntraSearch* pSearch = ctx->m_pSearch;
    TransformUnit& tu = *ctx->m_pTu;
    const ComponentID compID = ctx->m_compId;
    CodingStructure& cs = *ctx->m_pCs;
    const Slice& slice = *cs.slice;

    // ── Restore CABAC ──
    if (ctx->m_pCtxStart)
    {
        pSearch->m_CABACEstimator->getCtx() = *ctx->m_pCtxStart;
        pSearch->m_CABACEstimator->resetBits();
    }

    // ── Per-TU field init (preserve caller-set jointCbCr) ──
    bool jointCbCr = tu.jointCbCr && compID == COMP_Cb;
    ctx->m_codeCompId = compID;
    ctx->m_codedCbfMask = 0;

    if (isChroma(compID))
    {
        tu.cbf[1] = 0;
        tu.cbf[2] = 0;
    }

    // ── Lambda selection & scaling ──
    pSearch->m_pcTrQuant->selectLambda(compID);

    bool lmcsFlag = cs.picHeader->lmcsEnabled && (slice.isIntra() || (!slice.isIntra() && cs.picture->reshapeData.getCTUFlag()));
    bool lmcsResidScale = lmcsFlag && (tu.blocks[compID].width * tu.blocks[compID].height > 4);

    if (lmcsResidScale && isChroma(compID) && cs.picHeader->lmcsChromaResidualScale)
    {
        int cResScaleInv = tu.chromaAdj;
        double cRescale = (double)(1 << CSCALE_FP_PREC) / (double)cResScaleInv;
        pSearch->m_pcTrQuant->scaleLambda(1.0 / (cRescale * cRescale));
    }

    jointCbCr = tu.jointCbCr && compID == COMP_Cb;
    if (jointCbCr)
    {
        int absIct = abs(TU::getICTMode(tu));
        double lfact = (absIct == 1 || absIct == 3 ? 0.8 : 0.5);
        pSearch->m_pcTrQuant->scaleLambda(lfact);
    }
    if (cs.sps->jointCbCr && isChroma(compID) && (tu.cu->cs->slice->sliceQp > 18))
    {
        pSearch->m_pcTrQuant->scaleLambda(1.3);
    }

    // ── Luma path ──
    if (isLuma(compID))
    {
        TCoeff absSum = 0;
        const QpParam cQP(tu, compID);
        pSearch->m_pcTrQuant->transformNxN(
            tu, compID, cQP, absSum,
            pSearch->m_CABACEstimator->getCtx(), ctx->m_loadTr);

        ctx->m_absSum = (int)absSum;

        // ISP all-zero CBF check → early return
        if (tu.cu->ispMode && CU::isISPLast(*tu.cu, tu.blocks[compID], tu.blocks[compID].compID)
            && CU::allLumaCBFsAreZero(*tu.cu))
        {
            ctx->m_bSkip = true;
            ctx->m_dist = MAX_INT;
            return true;
        }

        // Copy coefficients for INV_XFORM
        CoeffSigBuf coeffBuf = tu.getCoeffs(compID);
        int numCoeff = tu.blocks[compID].area();
        for (int i = 0; i < numCoeff && i < MAX_TB_SIZEY * MAX_TB_SIZEY; i++)
        {
            ctx->m_coeffs[i] = coeffBuf.buf[i];
        }
    }
    // ── Chroma path ──
    else
    {
        PelBuf crPred = cs.getPredBuf(COMP_Cr);
        PelBuf crResi = cs.getResiBuf(COMP_Cr);
        PelBuf crReco = cs.getRecoBuf(COMP_Cr);

        ctx->m_crPred = crPred;
        ctx->m_crResi = crResi;
        ctx->m_crReco = crReco;

        ctx->m_codeCompId = (tu.jointCbCr ? (tu.jointCbCr >> 1 ? COMP_Cb : COMP_Cr) : compID);
        const QpParam qpCbCr(tu, ctx->m_codeCompId);

        if (tu.jointCbCr)
        {
            ComponentID otherCompId = (ctx->m_codeCompId == COMP_Cr ? COMP_Cb : COMP_Cr);
            tu.getCoeffs(otherCompId).fill(0);
            TU::setCbfAtDepth(tu, otherCompId, tu.depth, false);
        }

        PelBuf& codeResi = (ctx->m_codeCompId == COMP_Cr ? crResi : ctx->m_resiBuf);
        TCoeff absSum = 0;

        pSearch->m_pcTrQuant->transformNxN(
            tu, ctx->m_codeCompId, qpCbCr, absSum,
            pSearch->m_CABACEstimator->getCtx(), ctx->m_loadTr);

        ctx->m_absSum = (int)absSum;

        // Copy coeffs for codeCompId
        {
            CoeffSigBuf coeffBuf = tu.getCoeffs(ctx->m_codeCompId);
            int numCoeff = tu.blocks[ctx->m_codeCompId].area();
            Pel* dest = (ctx->m_codeCompId == COMP_Cr) ? ctx->m_crCoeffs : ctx->m_coeffs;
            for (int i = 0; i < numCoeff && i < MAX_TB_SIZEY * MAX_TB_SIZEY; i++)
            {
                dest[i] = coeffBuf.buf[i];
            }
        }

        if (absSum > 0)
        {
            pSearch->m_pcTrQuant->invTransformNxN(tu, ctx->m_codeCompId, codeResi, qpCbCr);
            ctx->m_codedCbfMask += (ctx->m_codeCompId == COMP_Cb ? 2 : 1);
        }
        else
        {
            codeResi.fill(0);
        }

        // jointCbCr: check mask and ICT
        if (tu.jointCbCr)
        {
            if (tu.jointCbCr == 3 && ctx->m_codedCbfMask == 2)
            {
                ctx->m_codedCbfMask = 3;
                TU::setCbfAtDepth(tu, COMP_Cr, tu.depth, true);
            }
            if (tu.jointCbCr != ctx->m_codedCbfMask)
            {
                ctx->m_bSkip = true;
                ctx->m_dist = MAX_DISTORTION;
                return true;
            }
            pSearch->m_pcTrQuant->invTransformICT(tu, ctx->m_resiBuf, crResi);
            ctx->m_absSum = ctx->m_codedCbfMask;
        }

        // LMCS chroma residual scaling
        if (lmcsFlag && ctx->m_absSum > 0 && cs.picHeader->lmcsChromaResidualScale)
        {
            ctx->m_resiBuf.scaleSignal(tu.chromaAdj, 0, slice.clpRngs[compID]);
            if (tu.jointCbCr)
            {
                crResi.scaleSignal(tu.chromaAdj, 0, slice.clpRngs[COMP_Cr]);
            }
        }
    }

    return true;
}

// ═══════════════════════════════════════════════════════════════════
// Stage 4: INV_XFORM (luma only)
// ═══════════════════════════════════════════════════════════════════
// Inverse transform + dequant. LUMA ONLY — chroma FWD_XFORM already
// includes both forward + inverse transform (matching the inline path
// in xIntraCodingTUBlock lines 1429-1511).
// No CABAC dependency. Reads m_coeffs, writes m_resiBuf.
bool SchedulerExecutors::execInvXform(WorkUnit* pWu, void* pScratch)
{
    (void)pScratch;
    if (!pWu || !pWu->m_pCtx) return false;

    TuStageData* ctx = (TuStageData*)pWu->m_pCtx;
    if (!ctx->m_pSearch || !ctx->m_pTu) return false;

    // Early return from FWD_XFORM → nothing to inverse-transform
    if (ctx->m_bSkip) return true;

    // Chroma: FWD_XFORM already did forward+inverse (inline style)
    if (!isLuma(ctx->m_compId)) return true;

    IntraSearch* pSearch = ctx->m_pSearch;
    TransformUnit& tu = *ctx->m_pTu;
    const ComponentID compID = ctx->m_compId;

    // Write coefficients back to TU
    CoeffSigBuf coeffBuf = tu.getCoeffs(compID);
    int numCoeff = tu.blocks[compID].area();
    for (int i = 0; i < numCoeff && i < MAX_TB_SIZEY * MAX_TB_SIZEY; i++)
    {
        coeffBuf.buf[i] = ctx->m_coeffs[i];
    }

    if (ctx->m_absSum > 0)
    {
        const QpParam cQP(tu, compID);
        pSearch->m_pcTrQuant->invTransformNxN(tu, compID, ctx->m_resiBuf, cQP);
    }
    else
    {
        ctx->m_resiBuf.fill(0);
    }

    return true;
}

// ═══════════════════════════════════════════════════════════════════
// Stage 5: RECONSTRUCT + DISTORTION
// ═══════════════════════════════════════════════════════════════════
// reco = pred + resi, then SSE distortion.
// Handles 4 distortion sub-paths: LMCS luma, LMCS chroma, weighted, plain.
// No CABAC dependency. Writes m_dist + m_recoBuf.
bool SchedulerExecutors::execReconstruct(WorkUnit* pWu, void* pScratch)
{
    (void)pScratch;
    if (!pWu || !pWu->m_pCtx) return false;

    TuStageData* ctx = (TuStageData*)pWu->m_pCtx;
    if (!ctx->m_pSearch || !ctx->m_pTu) return false;

    // Reconstruct is a no-op if FWD_XFORM had an early return
    if (ctx->m_bSkip) return true;

    IntraSearch* pSearch = ctx->m_pSearch;
    TransformUnit& tu = *ctx->m_pTu;
    const ComponentID compID = ctx->m_compId;
    const CodingStructure& cs = *ctx->m_pCs;
    const Slice& slice = *cs.slice;
    const ReshapeData& reshapeData = cs.picture->reshapeData;

    // ── Reconstruction ──
    {
        const ClpRng& clpRng = slice.clpRngs[compID];
        ctx->m_recoBuf.reconstruct(ctx->m_predBuf, ctx->m_resiBuf, clpRng);
    }

    // Chroma jointCbCr: reconstruct Cr
    bool jointCbCr = tu.jointCbCr && compID == COMP_Cb;
    if (jointCbCr)
    {
        ctx->m_crReco.reconstruct(ctx->m_crPred, ctx->m_crResi, slice.clpRngs[COMP_Cr]);
    }

    // ── Distortion ──
    {
        const int bitDepth = cs.sps->bitDepths[toChannelType(compID)];
        bool lmcsEnabled = cs.picHeader->lmcsEnabled && (reshapeData.getCTUFlag() || (isChroma(compID) && pSearch->m_pcEncCfg->m_reshapeSignalType == RESHAPE_SIGNAL_PQ));
        bool lumaLevelToDeltaQP = pSearch->m_pcEncCfg->m_lumaLevelToDeltaQPEnabled;

        if (lmcsEnabled || lumaLevelToDeltaQP)
        {
            const CPelBuf orgLuma = cs.getOrgBuf(cs.area.blocks[COMP_Y]);

            if (compID == COMP_Y && !lumaLevelToDeltaQP)
            {
                CPelBuf tmpRecLuma = cs.getRspRecoBuf(tu.blocks[compID]);
                tmpRecLuma.rspSignal(ctx->m_recoBuf, reshapeData.getInvLUT());
                ctx->m_dist += pSearch->m_pcRdCost->getDistPart(
                    ctx->m_orgBufC, tmpRecLuma, bitDepth, compID, DF_SSE_WTD, &orgLuma);
            }
            else
            {
                ctx->m_dist += pSearch->m_pcRdCost->getDistPart(
                    ctx->m_orgBufC, ctx->m_recoBuf, bitDepth, compID, DF_SSE_WTD, &orgLuma);
                if (jointCbCr)
                {
                    CPelBuf crOrg = cs.getOrgBuf(COMP_Cr);
                    ctx->m_dist += pSearch->m_pcRdCost->getDistPart(
                        crOrg, ctx->m_crReco, bitDepth, COMP_Cr, DF_SSE_WTD, &orgLuma);
                }
            }
        }
        else
        {
            ctx->m_dist += pSearch->m_pcRdCost->getDistPart(
                ctx->m_orgBufC, ctx->m_recoBuf, bitDepth, compID, DF_SSE);
            if (jointCbCr)
            {
                CPelBuf crOrg = cs.getOrgBuf(COMP_Cr);
                ctx->m_dist += pSearch->m_pcRdCost->getDistPart(
                    crOrg, ctx->m_crReco, bitDepth, COMP_Cr, DF_SSE);
            }
        }
    }

    // ── Propagate to caller outputs ──
    if (ctx->m_pOutNumSig)
    {
        CoeffSigBuf coeffs = tu.getCoeffs(compID);
        uint32_t sig = 0;
        for (int i = 0; i < tu.blocks[compID].area(); i++)
        {
            if (coeffs.buf[i]) sig++;
        }
        *ctx->m_pOutNumSig = sig;
    }
    if (ctx->m_pOutPredBuf && isLuma(ctx->m_compId))
    {
        ctx->m_pOutPredBuf->Y().copyFrom(ctx->m_predBuf);
    }

    return true;
}

// ═══════════════════════════════════════════════════════════════════
// Inter executor (unchanged)
// ═══════════════════════════════════════════════════════════════════
bool SchedulerExecutors::execInterTu(WorkUnit* pWu, void* pScratch)
{
    (void)pScratch;
    if (!pWu || !pWu->m_pCtx) return false;

    InterTuExecCtx* interCtx = (InterTuExecCtx*)pWu->m_pCtx;
    if (!interCtx->pSearch || !interCtx->pCs) return false;

    if (interCtx->pCtxStart)
    {
        interCtx->pSearch->m_CABACEstimator->getCtx() = *interCtx->pCtxStart;
        interCtx->pSearch->m_CABACEstimator->resetBits();
    }

    Distortion* pDist = (Distortion*)interCtx->pZeroDist;
    interCtx->pSearch->xEstimateInterResidualQT(
        *interCtx->pCs, *interCtx->pPartitioner, pDist);

    return true;
}

}
