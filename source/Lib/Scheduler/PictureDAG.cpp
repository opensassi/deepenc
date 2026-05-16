/** \file     PictureDAG.cpp
    \brief    Wavefront CTU dependency graph builder implementation
 */

#include "PictureDAG.h"
#include "WorkUnit.h"

#include "CommonLib/Slice.h"
#include "CommonLib/Picture.h"
#include "CommonLib/Unit.h"

#ifdef VVENC_SOURCE
#include "EncoderLib/EncSlice.h"
#endif

namespace vvenc {

#ifdef VVENC_SOURCE
bool execCtuStage(WorkUnit* pWu, void* pScratch)
{
    (void)pScratch;
    if (!pWu || !pWu->m_pCtx) return false;
    CtuExecCtx* ctx = (CtuExecCtx*)pWu->m_pCtx;
    if (!ctx->pEncSlice || !ctx->pPic) return false;
    return ctx->pEncSlice->xProcessCtuStage(ctx->pPic,
        ctx->ctuRsAddr, ctx->ctuPosX, ctx->ctuPosY, pWu->m_eStage);
}
#endif

struct CtuStageDef
{
    Stage   stage;
    uint8_t spatialDepMask;
    int8_t  requiredNeighborStage;
};

static const CtuStageDef s_ctuStages[] =
{
    { Stage::CTU_ENCODE,   SPATIAL_LEFT | SPATIAL_TOP | SPATIAL_TOP_RIGHT, WF_RECON_WRITE },
    { Stage::RECON_WRITE,  0,                                             WF_NOT_READY    },
    { Stage::LF_VER,       SPATIAL_RIGHT | SPATIAL_BOT_RIGHT,             WF_RECON_WRITE  },
    { Stage::LF_HOR,       SPATIAL_TOP,                                    WF_LF_VER       },
    { Stage::SAO_FILTER,   SPATIAL_TOP,                                    WF_LF_HOR       },
    { Stage::ALF_STATS,    0,                                              WF_NOT_READY    },
    { Stage::ALF_DERIVE,   0,                                              WF_NOT_READY    },
    { Stage::ALF_RECON,    0,                                              WF_NOT_READY    },
    { Stage::CCALF_STATS,  0,                                              WF_NOT_READY    },
    { Stage::CCALF_DERIVE, 0,                                              WF_NOT_READY    },
    { Stage::CCALF_RECON,  0,                                              WF_NOT_READY    },
};

static const int NUM_CTU_STAGES = sizeof(s_ctuStages) / sizeof(s_ctuStages[0]);

int PictureDAG::estimatePoolSize(const Slice& slice)
{
    if (!slice.pps || !slice.pps->pcv)
    {
        return 0;
    }
    return (int)slice.pps->pcv->sizeInCtus * NUM_CTU_STAGES;
}

int PictureDAG::build(Slice& slice, Picture* pic,
                       WorkUnit* pPool, int poolSize, int& numUnits,
                       std::atomic<int8_t>* pCtuStates)
{
    numUnits = 0;

    if (!pic || !pic->cs || !pic->cs->pcv)
    {
        return -2;
    }

    const PreCalcValues* pcv = pic->cs->pcv;
    if (pcv->sizeInCtus == 0)
    {
        return -2;
    }

    int numCtus = (int)pcv->sizeInCtus;
    int numCols = (int)pcv->widthInCtus;
    int numRows = (int)pcv->heightInCtus;

    int estimated = estimatePoolSize(slice);
    if (estimated > poolSize)
    {
        return -1;
    }

    WorkUnit* pNext = pPool;

    for (int rsAddr = 0; rsAddr < numCtus; rsAddr++)
    {
        uint16_t posX = (uint16_t)(rsAddr % numCols);
        uint16_t posY = (uint16_t)(rsAddr / numCols);

        int ret = xAddCtuEncode((uint32_t)rsAddr, posX, posY,
                                 pNext, numUnits,
                                 pCtuStates, numCols, numRows);
        if (ret < 0) return ret;
    }

    return 0;
}

int PictureDAG::xAddCtuEncode(uint32_t rsAddr, uint16_t posX, uint16_t posY,
                                WorkUnit*& pNext, int& numUnits,
                                std::atomic<int8_t>* pCtuStates,
                                int numCtuCols, int numCtuRows)
{
    (void)pCtuStates;
    (void)numCtuRows;

    WorkUnit* pPrev = nullptr;

    for (int s = 0; s < NUM_CTU_STAGES; s++)
    {
        WorkUnit* pWu = pNext++;
        numUnits++;

        pWu->m_eStage        = s_ctuStages[s].stage;
        pWu->m_tuId          = rsAddr;
        pWu->m_compId        = 0;
        pWu->m_width         = 0;
        pWu->m_height        = 0;
        pWu->m_qp            = 0;
        pWu->m_mtsIdx        = 0;
        pWu->m_ctuRsAddr     = rsAddr;
        pWu->m_ctuPosX       = posX;
        pWu->m_ctuPosY       = posY;
        pWu->m_spatialDepMask = s_ctuStages[s].spatialDepMask;
        pWu->m_pDependents   = nullptr;
        pWu->m_numDependents = 0;
        pWu->m_pInputBuf     = nullptr;
        pWu->m_pOutputBuf    = nullptr;
        pWu->m_pScratch      = nullptr;
#ifdef VVENC_SOURCE
        pWu->m_pfnExec       = execCtuStage;
#endif

        if (pPrev)
        {
            xLinkStages(pPrev, pWu);
        }

        pPrev = pWu;
    }

    return 0;
}

void PictureDAG::xLinkStages(WorkUnit* pPrev, WorkUnit* pNext)
{
    WorkUnit** oldDeps = pPrev->m_pDependents;
    int oldNum = pPrev->m_numDependents;

    WorkUnit** newDeps = new WorkUnit*[oldNum + 1];
    for (int i = 0; i < oldNum; i++)
    {
        newDeps[i] = oldDeps[i];
    }
    newDeps[oldNum] = pNext;

    delete[] oldDeps;
    pPrev->m_pDependents = newDeps;
    pPrev->m_numDependents = oldNum + 1;

    pNext->m_depCount.fetch_add(1, std::memory_order_acq_rel);
}

bool PictureDAG::checkSpatialDeps(uint32_t ctuRsAddr,
                                   uint16_t ctuPosX, uint16_t ctuPosY,
                                   uint8_t depMask, int8_t requiredStage,
                                   const std::atomic<int8_t>* pCtuStates,
                                   int numCtuCols)
{
    if (depMask == 0)
    {
        return true;
    }

    if (!pCtuStates)
    {
        return true;
    }

    if (depMask & SPATIAL_LEFT)
    {
        if (ctuPosX > 0)
        {
            int neighborAddr = (int)ctuRsAddr - 1;
            if (pCtuStates[neighborAddr].load(std::memory_order_acquire) < requiredStage)
            {
                return false;
            }
        }
    }

    if (depMask & SPATIAL_TOP)
    {
        if (ctuPosY > 0)
        {
            int neighborAddr = (int)ctuRsAddr - numCtuCols;
            if (pCtuStates[neighborAddr].load(std::memory_order_acquire) < requiredStage)
            {
                return false;
            }
        }
    }

    if (depMask & SPATIAL_TOP_RIGHT)
    {
        if (ctuPosY > 0)
        {
            int neighborAddr = (int)ctuRsAddr - numCtuCols + 1;
            if (pCtuStates[neighborAddr].load(std::memory_order_acquire) < requiredStage)
            {
                return false;
            }
        }
    }

    if (depMask & SPATIAL_BOT_RIGHT)
    {
        int neighborAddr = (int)ctuRsAddr + numCtuCols + 1;
        if (neighborAddr >= 0 && pCtuStates[neighborAddr].load(std::memory_order_acquire) < requiredStage)
        {
            return false;
        }
    }

    if (depMask & SPATIAL_RIGHT)
    {
        int neighborAddr = (int)ctuRsAddr + 1;
        if (pCtuStates[neighborAddr].load(std::memory_order_acquire) < requiredStage)
        {
            return false;
        }
    }

    return true;
}

int8_t PictureDAG::xRequiredNeighborStage(Stage stage)
{
    for (int s = 0; s < NUM_CTU_STAGES; s++)
    {
        if (s_ctuStages[s].stage == stage)
        {
            return s_ctuStages[s].requiredNeighborStage;
        }
    }
    return WF_NOT_READY;
}

PictureDAG::~PictureDAG() = default;

}
