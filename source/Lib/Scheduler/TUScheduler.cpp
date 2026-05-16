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
#endif

#include <cstdlib>
#include <cstring>

#include "SchedulerTrace.h"

namespace vvenc {

TUScheduler* g_pSchedulerTraceTarget = nullptr;
SchedulerTrace* g_pSchedulerTrace = nullptr;
TUScheduler* g_pScheduler = nullptr;
bool g_schedulerActive = false;

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
int TUScheduler::xCalcPoolSize(CodingUnit* pCu)
{
    return TUPipelineDAG::estimatePoolSize(pCu);
}

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

    while (remaining > 0)
    {
        int completed = 0;
        ret = xSubmitReady(m_pWorkPool, numUnits, completed);
        if (ret < 0) break;
        remaining -= completed;
    }

    return 0;
}

#ifdef VVENC_SOURCE
int TUScheduler::submitModeTrial(CodingUnit* pCu)
{
    if (!m_bInitialized)
    {
        return -1;
    }

    int poolSize = xCalcPoolSize(pCu);
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
    int ret = TUPipelineDAG::build(pCu, m_pWorkPool, poolSize, numUnits);
    if (ret < 0)
    {
        return -2;
    }

    int remaining = numUnits;

    while (remaining > 0)
    {
        int completed = 0;
        ret = xSubmitReady(m_pWorkPool, numUnits, completed);
        if (ret < 0) break;
        remaining -= completed;
    }

    return 0;
}
#endif

int TUScheduler::xSubmitReady(WorkUnit* pUnits, int numUnits, int& completed)
{
    completed = 0;

    for (int i = 0; i < numUnits; i++)
    {
        WorkUnit* pWu = &pUnits[i];
        if (pWu->m_depCount.load(std::memory_order_acquire) != 0)
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
                (int8_t)((int)pWu->m_eStage - 1),
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
        pDep->m_depCount.fetch_sub(1, std::memory_order_acq_rel);
    }

    completed++;

    TUScheduler* sched = g_pSchedulerTraceTarget;
    if (sched && sched->m_pCtuStates)
    {
        int stageVal = (int)pWu->m_eStage;
        int rsAddr = (int)pWu->m_ctuRsAddr;
        if (stageVal > (int)WF_NOT_READY && stageVal <= (int)WF_DONE)
        {
            sched->m_pCtuStates[rsAddr].store((int8_t)stageVal, std::memory_order_release);
        }
    }

    delete[] pWu->m_pDependents;
    pWu->m_pDependents = nullptr;
    pWu->m_numDependents = 0;
}

#ifdef VVENC_SOURCE
int TUScheduler::submitFrame(Slice& slice, Picture* pic)
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
