/** \file     SchedulerExecutors.cpp
    \brief    Executor functions wrapping real encoder pipeline stages
 */

#include "SchedulerExecutors.h"
#include "WorkUnit.h"

#include "EncoderLib/IntraSearch.h"
#include "EncoderLib/InterSearch.h"
#include "CommonLib/CodingStructure.h"
#include "CommonLib/Contexts.h"
#include "CommonLib/Unit.h"
#include "CommonLib/TypeDef.h"

namespace vvenc {

extern bool g_schedulerActive;

bool SchedulerExecutors::execIntraTu(WorkUnit* pWu, void* pScratch)
{
    (void)pScratch;
    if (!pWu || !pWu->m_pCtx) return false;



    IntraTuExecCtx* ctx = (IntraTuExecCtx*)pWu->m_pCtx;
    if (!ctx->pSearch || !ctx->pTu) return false;

    // Restore CABAC context to snapshot taken before DAG build
    if (ctx->pCtxStart)
    {
        ctx->pSearch->m_CABACEstimator->getCtx() = *ctx->pCtxStart;
        ctx->pSearch->m_CABACEstimator->resetBits();
    }

    TransformUnit* pTu = (TransformUnit*)ctx->pTu;
    ComponentID compId = (ComponentID)ctx->compId;
    Distortion dist = 0;

    // Per-TU field init matching inline callers (IntraSearch.cpp:~2075)
    pTu->jointCbCr = 0;
    if (isChroma(compId))
    {
        pTu->cbf[1] = 0;
        pTu->cbf[2] = 0;
    }

    PelUnitBuf* pPred = (PelUnitBuf*)ctx->pPred;

    ctx->pSearch->xIntraCodingTUBlock(
        *pTu, compId, ctx->checkCrossCPrediction,
        dist, ctx->pNumSig, pPred, ctx->loadTr
    );

    if (ctx->pDist)
    {
        *ctx->pDist = dist;
    }

    return true;
}

bool SchedulerExecutors::execInterTu(WorkUnit* pWu, void* pScratch)
{
    (void)pScratch;
    if (!pWu || !pWu->m_pCtx) return false;

    InterTuExecCtx* ctx = (InterTuExecCtx*)pWu->m_pCtx;
    if (!ctx->pSearch || !ctx->pCs) return false;

    // Restore CABAC context to snapshot taken before DAG build
    if (ctx->pCtxStart)
    {
        ctx->pSearch->m_CABACEstimator->getCtx() = *ctx->pCtxStart;
        ctx->pSearch->m_CABACEstimator->resetBits();
    }

    Distortion* pDist = (Distortion*)ctx->pZeroDist;
    ctx->pSearch->xEstimateInterResidualQT(
        *ctx->pCs, *ctx->pPartitioner, pDist
    );

    return true;
}

int SchedulerExecutors::setupIntraTu(IntraSearch* pSearch,
                                      WorkUnit* pPool, int numUnits)
{
    if (!pSearch || !pPool) return -1;

    for (int i = 0; i < numUnits; i++)
    {
        pPool[i].m_pfnExec = execIntraTu;
    }

    return 0;
}

}
