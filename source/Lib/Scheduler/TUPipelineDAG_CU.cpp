/** \file     TUPipelineDAG_CU.cpp
    \brief    Real CU/TU DAG builder (encoder build only)
 */

#include "TUPipelineDAG.h"
#include "WorkUnit.h"

#include "CommonLib/CodingStructure.h"
#include "CommonLib/Unit.h"

namespace vvenc {

static constexpr int STAGES_PER_COMPONENT = 7;

static Stage s_componentStages[STAGES_PER_COMPONENT] =
{
    Stage::INIT_PRED,
    Stage::PREDICT,
    Stage::RESIDUAL,
    Stage::FWD_XFORM,
    Stage::QUANT_FILL,
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

    for (TransformUnit* pTu = pCu->firstTU; pTu; pTu = pTu->next)
    {
        WorkUnit* pLastInTu = nullptr;

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
                pWu->m_tuId          = pTu->idx;
                pWu->m_compId        = (uint8_t)compId;
                pWu->m_width         = pTu->blocks[c].width;
                pWu->m_height        = pTu->blocks[c].height;
                pWu->m_qp            = (int8_t)pCu->qp;
                pWu->m_mtsIdx        = (uint8_t)pTu->mtsIdx[c];
                pWu->m_bCbf          = pTu->cbf[c] ? true : false;
                pWu->m_spatialDepMask = 0;
                pWu->m_ctuRsAddr     = 0;
                pWu->m_ctuPosX       = 0;
                pWu->m_ctuPosY       = 0;
                pWu->m_pDependents   = nullptr;
                pWu->m_numDependents = 0;
                pWu->m_pInputBuf     = nullptr;
                pWu->m_pOutputBuf    = nullptr;
                pWu->m_pScratch      = nullptr;
                pWu->m_pfnExec       = nullptr;

                if (pPrev)
                {
                    WorkUnit** oldDeps = pPrev->m_pDependents;
                    int oldNum = pPrev->m_numDependents;
                    WorkUnit** newDeps = new WorkUnit*[oldNum + 1];
                    for (int i = 0; i < oldNum; i++) newDeps[i] = oldDeps[i];
                    newDeps[oldNum] = pWu;
                    delete[] oldDeps;
                    pPrev->m_pDependents = newDeps;
                    pPrev->m_numDependents = oldNum + 1;
                    pWu->m_depCount.fetch_add(1, std::memory_order_acq_rel);
                }

                if (s == 0 && pLastInTu)
                {
                    pWu->m_depCount.fetch_add(1, std::memory_order_acq_rel);
                    WorkUnit** oldDeps = pLastInTu->m_pDependents;
                    int oldNum = pLastInTu->m_numDependents;
                    WorkUnit** newDeps = new WorkUnit*[oldNum + 1];
                    for (int i = 0; i < oldNum; i++) newDeps[i] = oldDeps[i];
                    newDeps[oldNum] = pWu;
                    delete[] oldDeps;
                    pLastInTu->m_pDependents = newDeps;
                    pLastInTu->m_numDependents = oldNum + 1;
                }

                pPrev = pWu;
            }

            pLastInTu = pPrev;
        }
    }

    return 0;
}

int TUPipelineDAG::estimatePoolSize(CodingUnit* pCu)
{
    if (!pCu) return 0;

    int total = 0;
    for (TransformUnit* pTu = pCu->firstTU; pTu; pTu = pTu->next)
    {
        int comps = 0;
        for (int c = 0; c < MAX_NUM_TBLOCKS; c++)
        {
            if (c == 0 || pTu->cbf[c])
            {
                comps++;
            }
        }
        total += comps * STAGES_PER_COMPONENT;
    }
    return total;
}

}
