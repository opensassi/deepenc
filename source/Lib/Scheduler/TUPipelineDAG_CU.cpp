/** \file     TUPipelineDAG_CU.cpp
    \brief    Real CU/TU DAG builder (encoder build only)
 */

#include "TUPipelineDAG.h"
#include "WorkUnit.h"
#include "SchedulerExecutors.h"
#include "TuStageData.h"

#include "CommonLib/CodingStructure.h"
#include "CommonLib/Unit.h"

namespace vvenc {

static constexpr int STAGES_PER_COMPONENT = 5;

static Stage s_componentStages[STAGES_PER_COMPONENT] =
{
    Stage::INIT_PRED,
    Stage::RESIDUAL,
    Stage::FWD_XFORM,
    Stage::INV_XFORM,
    Stage::RECONSTRUCT
};

int TUPipelineDAG::build(CodingUnit* pCu,
                          WorkUnit* pPool, int poolSize, int& numUnits)
{
    numUnits = 0;

    if (!pCu)
    {
        return -2;
    }

    int estimated = estimatePoolSize(pCu);
    if (estimated > poolSize)
    {
        return -1;
    }

    WorkUnit* pNext = pPool;
    uint32_t tuIdx = 0;

    for (TransformUnit* pTu = pCu->firstTU; pTu; pTu = pTu->next)
    {
        WorkUnit* pLastInTu = nullptr;
        uint32_t thisTuId = tuIdx++;

        for (int c = 0; c < MAX_NUM_TBLOCKS; c++)
        {
            ComponentID compId = (ComponentID)c;
            if (!pTu->cbf[c] && c > 0)
            {
                continue;
            }

            WorkUnit* pPrev = nullptr;

            for (int s = 0; s < STAGES_PER_COMPONENT; s++)
            {
                if (numUnits >= poolSize)
                {
                    return -1;
                }

                WorkUnit* pWu = pNext++;
                numUnits++;

                pWu->m_eStage        = s_componentStages[s];
                pWu->m_tuId          = thisTuId;
                pWu->m_compId        = (uint8_t)compId;
                pWu->m_width         = pTu->blocks[c].width;
                pWu->m_height        = pTu->blocks[c].height;
                pWu->m_qp            = (int8_t)pCu->qp;
                pWu->m_mtsIdx        = (uint8_t)pTu->mtsIdx[c];
                pWu->m_bCbf          = pTu->cbf[c] ? true : false;
                pWu->m_spatialDepMask  = 0;
                pWu->m_ctuRsAddr      = 0;
                pWu->m_ctuPosX        = 0;
                pWu->m_ctuPosY        = 0;
                pWu->m_numDependents  = 0;
                pWu->m_pInputBuf      = nullptr;
                pWu->m_pOutputBuf     = nullptr;
                pWu->m_pScratch       = nullptr;
                pWu->m_pfnExec        = nullptr;

                if (pPrev)
                {
                    CHECK(pPrev->m_numDependents >= WorkUnit::MAX_DEPS, "WorkUnit dependency overflow");
                    pPrev->m_pDependents[pPrev->m_numDependents++] = pWu;
                    pWu->m_depCount.fetch_add(1, std::memory_order_acq_rel);
                }

                if (s == 0 && pLastInTu)
                {
                    CHECK(pLastInTu->m_numDependents >= WorkUnit::MAX_DEPS, "WorkUnit dependency overflow");
                    pLastInTu->m_pDependents[pLastInTu->m_numDependents++] = pWu;
                    pWu->m_depCount.fetch_add(1, std::memory_order_acq_rel);
                }

                pPrev = pWu;
            }

            pLastInTu = pPrev;
        }
    }

    return 0;
}

int TUPipelineDAG::build(TransformUnit* pTu, uint8_t compId,
                          WorkUnit* pPool, int poolSize, int& numUnits)
{
    numUnits = 0;

    if (!pTu)
    {
        return -2;
    }

    if (STAGES_PER_COMPONENT > poolSize)
    {
        return -1;
    }

    WorkUnit* pNext = pPool;
    WorkUnit* pPrev = nullptr;

    for (int s = 0; s < STAGES_PER_COMPONENT; s++)
    {
        WorkUnit* pWu = pNext++;
        numUnits++;

        pWu->m_eStage        = s_componentStages[s];
        pWu->m_tuId          = (uint32_t)pTu->idx;
        pWu->m_compId        = compId;
        pWu->m_width         = pTu->blocks[compId].width;
        pWu->m_height        = pTu->blocks[compId].height;
        pWu->m_qp            = (int8_t)pTu->cu->qp;
        pWu->m_mtsIdx        = (uint8_t)pTu->mtsIdx[compId];
        pWu->m_bCbf          = (compId == 0 || pTu->cbf[compId]);
        pWu->m_spatialDepMask = 0;
        pWu->m_ctuRsAddr     = 0;
        pWu->m_ctuPosX       = 0;
        pWu->m_ctuPosY       = 0;
        pWu->m_pInputBuf     = nullptr;
        pWu->m_pOutputBuf    = nullptr;
        pWu->m_pScratch      = nullptr;
        pWu->m_pfnExec       = nullptr;
        pWu->m_numDependents = 0;

        if (pPrev)
        {
            CHECK(pPrev->m_numDependents >= WorkUnit::MAX_DEPS, "WorkUnit dependency overflow");
            pPrev->m_pDependents[pPrev->m_numDependents++] = pWu;
            pWu->m_depCount.fetch_add(1, std::memory_order_acq_rel);
        }

        pPrev = pWu;
    }

    return 0;
}

int TUPipelineDAG::estimatePoolSize(CodingUnit* pCu)
{
    if (!pCu) return 0;

    int total = 0;
    for (TransformUnit* pTu = pCu->firstTU; pTu; pTu = pTu->next)
    {
        total += MAX_NUM_TBLOCKS * STAGES_PER_COMPONENT;
    }
    return total;
}

int TUPipelineDAG::estimatePoolSize(TransformUnit* pTu, uint8_t compId)
{
    (void)pTu;
    (void)compId;
    return STAGES_PER_COMPONENT;
}

void TUPipelineDAG::wireExecutors(WorkUnit* pPool, int numUnits, const TuStageData* pStageData)
{
    for (int i = 0; i < numUnits; i++)
    {
        WorkUnit* pW = &pPool[i];
        pW->m_pfnExec = nullptr;
        if (pStageData)
        {
            pW->m_pCtx = (void*)&pStageData[pW->m_compId];
        }
        switch (pW->m_eStage)
        {
            case Stage::INIT_PRED:
                pW->m_pfnExec = SchedulerExecutors::execInitPred;
                break;
            case Stage::RESIDUAL:
                pW->m_pfnExec = SchedulerExecutors::execResidual;
                break;
            case Stage::FWD_XFORM:
                pW->m_pfnExec = SchedulerExecutors::execFwdXform;
                break;
            case Stage::INV_XFORM:
                pW->m_pfnExec = SchedulerExecutors::execInvXform;
                break;
            case Stage::RECONSTRUCT:
                pW->m_pfnExec = SchedulerExecutors::execReconstruct;
                break;
            default:
                break;
        }
    }
}

void TUPipelineDAG::xLink(WorkUnit* pPrev, WorkUnit* pNext)
{
    if (!pPrev || !pNext) return;
    CHECK(pPrev->m_numDependents >= WorkUnit::MAX_DEPS, "WorkUnit dependency overflow");
    pPrev->m_pDependents[pPrev->m_numDependents++] = pNext;
    pNext->m_depCount.fetch_add(1, std::memory_order_acq_rel);
}

}
