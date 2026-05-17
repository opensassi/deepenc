/** \file     TUScheduler.cpp
    \brief    TU pipeline dispatcher implementation
 */

#include "TUScheduler.h"
#include "RingBuffer.h"
#include "WorkUnit.h"
#include "TUPipelineDAG.h"
#include "PictureDAG.h"

#ifdef VVENC_SOURCE
#include "Utilities/NoMallocThreadPool.h"
#include "CommonLib/CodingStructure.h"
#include "CommonLib/Slice.h"
#include "CommonLib/Picture.h"
#include "PictureDAG.h"
#endif

#include <cstdlib>
#include <cstring>

#include "SchedulerTrace.h"

namespace vvenc {

TUScheduler* g_pSchedulerTraceTarget = nullptr;
SchedulerTrace* g_pSchedulerTrace = nullptr;
TUScheduler* g_pScheduler = nullptr;
bool g_schedulerActive = false;
int  g_schedulerDispatchCount = 0;
static bool s_schedulerDisabled = false;

bool vvencSchedulerDisabled() { return s_schedulerDisabled; }
void vvencSetSchedulerDisabled(bool disabled) { s_schedulerDisabled = disabled; }

int TUScheduler::init(NoMallocThreadPool* pPool, int windowSize)
{
    if (windowSize < 1)
    {
        return -2;
    }

    m_pPool = pPool;
    m_windowSize = windowSize;

    int slotSize = 64 * 64 * 4;
    m_pRing = new RingBuffer();
    int ret = m_pRing->init(slotSize, windowSize * 4);
    if (ret < 0)
    {
        delete m_pRing;
        m_pRing = nullptr;
        return ret;
    }

    m_bInitialized = true;

#if ENABLE_SCHEDULER_TRACE
    if (!g_pSchedulerTrace)
    {
        const char* pTraceFile = getenv("VVENC_SCHED_TRACE");
        if (pTraceFile)
        {
            g_pSchedulerTrace = new SchedulerTrace();
            if (g_pSchedulerTrace->init(pTraceFile) < 0)
            {
                delete g_pSchedulerTrace;
                g_pSchedulerTrace = nullptr;
            }
        }
    }
#endif

    return 0;
}

int TUScheduler::destroy()
{
    if (m_pRing)
    {
        m_pRing->destroy();
        delete m_pRing;
        m_pRing = nullptr;
    }
    if (m_pWorkPool)
    {
        delete[] m_pWorkPool;
        m_pWorkPool = nullptr;
    }
    if (m_pCtuStates)
    {
        delete[] m_pCtuStates;
        m_pCtuStates = nullptr;
    }
    m_poolSize = 0;
    m_numCtuInPic = 0;
    m_numCtuCols = 0;
    m_bFrameActive = false;
    m_bInitialized = false;
    g_pSchedulerTraceTarget = nullptr;
    return 0;
}

TUScheduler::~TUScheduler()
{
    destroy();
}

int TUScheduler::xCalcPoolSize(const MockTU* pTus, int numTus)
{
    return TUPipelineDAG::estimatePoolSize(pTus, numTus);
}

#ifdef VVENC_SOURCE
int TUScheduler::xCalcFramePoolSize(Slice& slice)
{
    return PictureDAG::estimatePoolSize(slice);
}
#endif

int TUScheduler::submitModeTrial(const MockTU* pTus, int numTus,
                                  void* pScratch, int scratchSize)
{
    if (!m_bInitialized)
    {
        return -1;
    }

    int poolSize = xCalcPoolSize(pTus, numTus);
    if (poolSize < 1)
    {
        return -2;
    }

    if (m_pWorkPool)
    {
        delete[] m_pWorkPool;
        m_pWorkPool = nullptr;
    }
    m_pWorkPool = new WorkUnit[poolSize];
    m_poolSize = poolSize;

    int numUnits = 0;
    int ret = TUPipelineDAG::build(pTus, numTus, m_pWorkPool, poolSize, numUnits);
    if (ret < 0)
    {
        return -2;
    }

    int remaining = numUnits;
    int prevRemaining = remaining;

    while (remaining > 0)
    {
        int completed = 0;
        ret = xSubmitReady(m_pWorkPool, numUnits, completed);
        if (ret < 0) break;
        remaining -= completed;
        if (remaining == prevRemaining && completed == 0)
        {
            break;
        }
        prevRemaining = remaining;
    }

    return 0;
}

int TUScheduler::executeWorkUnits(WorkUnit* pPool, int numUnits)
{
    if (!pPool || numUnits < 1) return -1;

    int remaining = numUnits;
    int prevRemaining = remaining;
    while (remaining > 0)
    {
        int completed = 0;
        int ret = xSubmitReady(pPool, numUnits, completed);
        if (ret < 0) break;
        remaining -= completed;
        if (remaining == prevRemaining && completed == 0)
        {
            break;
        }
        prevRemaining = remaining;
    }
    return 0;
}

int TUScheduler::xSubmitReady(WorkUnit* pUnits, int numUnits, int& completed)
{
    completed = 0;

    uint32_t activeTu = 0;
    if (m_ePolicy == BatchPolicy::TU_SEQUENTIAL)
    {
        uint32_t maxTu = 0;
        for (int i = 0; i < numUnits; i++)
        {
            if (pUnits[i].m_tuId > maxTu)
                maxTu = pUnits[i].m_tuId;
        }
        for (uint32_t tu = 0; tu <= maxTu; tu++)
        {
            bool allDone = true;
            for (int i = 0; i < numUnits; i++)
            {
                if (pUnits[i].m_tuId == tu && pUnits[i].m_depCount.load() >= 0)
                {
                    allDone = false;
                    break;
                }
            }
            if (!allDone)
            {
                activeTu = tu;
                break;
            }
            activeTu = tu + 1;
        }
    }

    for (int i = 0; i < numUnits; i++)
    {
        WorkUnit* pWu = &pUnits[i];
        if (pWu->m_depCount.load(std::memory_order_acquire) != 0)
        {
            continue;
        }

        if (m_ePolicy == BatchPolicy::TU_SEQUENTIAL && pWu->m_tuId > activeTu)
        {
            continue;
        }

        pWu->m_depCount.store(-1, std::memory_order_relaxed);

        if (pWu->m_pfnExec)
        {
            pWu->m_pfnExec(pWu, nullptr);
        }

        xOnComplete(pWu, completed);
    }

    return 0;
}

#ifdef VVENC_SOURCE
int TUScheduler::xSubmitFrameReady()
{
    if (!m_pCtuStates || !m_pWorkPool)
    {
        return -1;
    }

    int completed = 0;

    for (int i = 0; i < m_poolSize; i++)
    {
        WorkUnit* pWu = &m_pWorkPool[i];
        if (pWu->m_depCount.load(std::memory_order_acquire) != 0)
        {
            continue;
        }

        if (!PictureDAG::checkSpatialDeps(
                pWu->m_ctuRsAddr, pWu->m_ctuPosX, pWu->m_ctuPosY,
                pWu->m_spatialDepMask,
                PictureDAG::xRequiredNeighborStage(pWu->m_eStage),
                m_pCtuStates, m_numCtuCols))
        {
            continue;
        }

        int old = pWu->m_depCount.fetch_sub(1, std::memory_order_acq_rel);
        if (old != 0)
        {
            continue;
        }

        if (pWu->m_pfnExec)
        {
            pWu->m_pfnExec(pWu, nullptr);
        }
        xOnComplete(pWu, completed);
    }

    return completed;
}
#endif

void TUScheduler::xOnComplete(WorkUnit* pWu, int& completed)
{
    for (int i = 0; i < pWu->m_numDependents; i++)
    {
        WorkUnit* pDep = pWu->m_pDependents[i];
        if (pDep)
        {
            pDep->m_depCount.fetch_sub(1, std::memory_order_acq_rel);
        }
    }

    completed++;

    TUScheduler* sched = g_pSchedulerTraceTarget;
    if (sched && sched->m_pCtuStates)
    {
        int rsAddr = (int)pWu->m_ctuRsAddr;
        if (pWu->m_eStage >= Stage::CTU_ENCODE && pWu->m_eStage <= Stage::CCALF_RECON)
        {
            sched->m_pCtuStates[rsAddr].store(WF_DONE, std::memory_order_release);
        }
    }

    pWu->m_numDependents = 0;
}

#ifdef VVENC_SOURCE
int TUScheduler::submitFrame(Slice& slice, Picture* pic, EncSlice* pEncSlice)
{
    if (!m_bInitialized)
    {
        return -1;
    }

    if (!pic || !pic->cs || !pic->cs->pcv)
    {
        return -1;
    }

    const PreCalcValues* pcv = pic->cs->pcv;
    int numCtus = (int)pcv->sizeInCtus;
    int numCols = (int)pcv->widthInCtus;

    int poolSize = xCalcFramePoolSize(slice);
    if (poolSize < 1)
    {
        return -2;
    }

    if (m_pWorkPool)
    {
        delete[] m_pWorkPool;
        m_pWorkPool = nullptr;
    }
    m_pWorkPool = new WorkUnit[poolSize];
    m_poolSize = poolSize;

    if (m_pCtuStates)
    {
        delete[] m_pCtuStates;
    }
    m_pCtuStates = new std::atomic<int8_t>[numCtus];
    m_numCtuInPic = numCtus;
    m_numCtuCols = numCols;

    xInitCtuStates(slice);

    int numUnits = 0;
    int ret = PictureDAG::build(slice, pic, m_pWorkPool, poolSize, numUnits, m_pCtuStates);
    if (ret < 0)
    {
        return -2;
    }

    // Wire CTU stage executors to all WorkUnits
    for (int i = 0; i < numUnits; i++)
    {
        CtuExecCtx* pCtx = new CtuExecCtx();
        pCtx->pEncSlice  = pEncSlice;
        pCtx->pPic       = pic;
        pCtx->ctuRsAddr  = (int)m_pWorkPool[i].m_ctuRsAddr;
        pCtx->ctuPosX    = (int)m_pWorkPool[i].m_ctuPosX;
        pCtx->ctuPosY    = (int)m_pWorkPool[i].m_ctuPosY;
        m_pWorkPool[i].m_pfnExec = execCtuStage;
        m_pWorkPool[i].m_pCtx    = pCtx;
    }

    // Connect trace target so xOnComplete updates CtuStates
    g_pSchedulerTraceTarget = this;

    m_bFrameActive = true;

    return 0;
}

int TUScheduler::advanceFrame()
{
    if (!m_bInitialized || !m_bFrameActive)
    {
        return -1;
    }

    int dispatched = xSubmitFrameReady();

    if (!m_pCtuStates) return -1;

    int allDone = 0;
    for (int i = 0; i < m_numCtuInPic; i++)
    {
        if (m_pCtuStates[i].load(std::memory_order_acquire) >= WF_DONE)
        {
            allDone++;
        }
    }

    if (allDone >= m_numCtuInPic)
    {
        m_bFrameActive = false;
        // Free CtuExecCtx allocations
        for (int i = 0; i < m_poolSize; i++)
        {
            if (m_pWorkPool[i].m_pCtx)
            {
                delete (CtuExecCtx*)m_pWorkPool[i].m_pCtx;
                m_pWorkPool[i].m_pCtx = nullptr;
            }
        }
        return 1;
    }

    return 0;
}

int TUScheduler::xInitCtuStates(Slice& slice)
{
    if (!m_pCtuStates) return -1;
    if (!slice.pps || !slice.pps->pcv) return -1;

    const PreCalcValues* pcv = slice.pps->pcv;
    int numCtus = (int)pcv->sizeInCtus;

    for (int i = 0; i < numCtus; i++)
    {
        m_pCtuStates[i].store(WF_NOT_READY, std::memory_order_relaxed);
    }

    return 0;
}
#endif

int TUScheduler::setPolicy(BatchPolicy ePolicy)
{
    if (ePolicy > BatchPolicy::WAVEFRONT)
    {
        return -1;
    }
    m_ePolicy = ePolicy;
    return 0;
}

int TUScheduler::setWindowSize(int nTUs)
{
    if (nTUs < 1)
    {
        return -1;
    }
    m_windowSize = nTUs;
    return 0;
}

BatchPolicy TUScheduler::getPolicy() const
{
    return m_ePolicy;
}

int TUScheduler::getWindowSize() const
{
    return m_windowSize;
}

}
