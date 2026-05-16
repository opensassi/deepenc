/** \file     SchedulerExecutors.cpp
    \brief    Executor functions wrapping real encoder pipeline stages
 */

#include "SchedulerExecutors.h"
#include "WorkUnit.h"

#include "EncoderLib/IntraSearch.h"
#include "CommonLib/CodingStructure.h"
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

    TransformUnit* pTu = (TransformUnit*)ctx->pTu;
    ComponentID compId = (ComponentID)ctx->compId;
    Distortion dist = 0;

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
