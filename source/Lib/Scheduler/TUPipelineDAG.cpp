/** \file     TUPipelineDAG.cpp
    \brief    TU pipeline dependency graph builder (MockTU overload)
 */

#include "TUPipelineDAG.h"
#include "WorkUnit.h"

#include "CommonLib/CommonDef.h"
#include <cstddef>

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

int TUPipelineDAG::build(const MockTU* pTus, int numTus,
                          WorkUnit* pPool, int poolSize, int& numUnits)
{
    numUnits = 0;

    if (!pTus || numTus < 1)
    {
        return -2;
    }

    int estimated = estimatePoolSize(pTus, numTus);
    if (estimated > poolSize)
    {
        return -1;
    }

    WorkUnit* pNext = pPool;

    for (int tuIdx = 0; tuIdx < numTus; tuIdx++)
    {
        const MockTU& tu = pTus[tuIdx];
        WorkUnit* pLastInTu = nullptr;

        uint8_t comps[3];
        int numComps = 0;
        if (tu.compMask & 1)   comps[numComps++] = SCHED_COMP_Y;
        if (tu.compMask & 2)   comps[numComps++] = SCHED_COMP_Cb;
        if (tu.compMask & 4)   comps[numComps++] = SCHED_COMP_Cr;

        for (int c = 0; c < numComps; c++)
        {
            if (numUnits + STAGES_PER_COMPONENT > poolSize)
            {
                return -1;
            }

            WorkUnit* pPrev = nullptr;

            for (int s = 0; s < STAGES_PER_COMPONENT; s++)
            {
                WorkUnit* pWu = pNext++;
                numUnits++;

                pWu->m_eStage        = s_componentStages[s];
                pWu->m_tuId          = (uint32_t)tuIdx;
                pWu->m_compId        = comps[c];
                pWu->m_width         = tu.width;
                pWu->m_height        = tu.height;
                pWu->m_qp            = tu.qp;
                pWu->m_mtsIdx        = tu.mtsIdx;
                pWu->m_bCbf          = false;
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

                pPrev = pWu;
            }

            if (pLastInTu)
            {
                WorkUnit* pFirst = pNext - STAGES_PER_COMPONENT;
                CHECK(pLastInTu->m_numDependents >= WorkUnit::MAX_DEPS, "WorkUnit dependency overflow");
                pLastInTu->m_pDependents[pLastInTu->m_numDependents++] = pFirst;
                pFirst->m_depCount.fetch_add(1, std::memory_order_acq_rel);
            }

            pLastInTu = pPrev;
        }
    }

    return 0;
}

int TUPipelineDAG::estimatePoolSize(const MockTU* pTus, int numTus)
{
    int total = 0;
    for (int i = 0; i < numTus; i++)
    {
        int comps = 0;
        if (pTus[i].compMask & 1) comps++;
        if (pTus[i].compMask & 2) comps++;
        if (pTus[i].compMask & 4) comps++;
        total += comps * STAGES_PER_COMPONENT;
    }
    return total;
}

TUPipelineDAG::~TUPipelineDAG() = default;

}
