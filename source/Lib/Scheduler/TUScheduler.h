/** \file     TUScheduler.h
    \brief    TU pipeline dispatcher facade
 */

#pragma once

#include <cstdint>
#include <atomic>

namespace vvenc {

class SchedulerTrace;

extern SchedulerTrace* g_pSchedulerTrace;

class TUScheduler;

extern TUScheduler* g_pSchedulerTraceTarget;
extern TUScheduler* g_pScheduler;
extern bool g_schedulerActive;
extern int  g_schedulerDispatchCount;      // dispatch hook fire counter
bool vvencSchedulerDisabled();           // runtime gate: true = bypass scheduler init
void vvencSetSchedulerDisabled(bool disabled);

class NoMallocThreadPool;
class RingBuffer;
class CodingStructure;
class Slice;
class Picture;
class EncSlice;
struct WorkUnit;
struct MockTU;

enum class BatchPolicy : uint8_t
{
    TU_SEQUENTIAL,
    STAGE_GLOBAL,
    HYBRID,
    WAVEFRONT
};

class TUScheduler
{
public:
    int init(NoMallocThreadPool* pPool, int windowSize = 8);
    int initTrace(const char* pFilename);
    int destroy();

    int submitModeTrial(const MockTU* pTus, int numTus,
                        void* pScratch, int scratchSize);

    int executeWorkUnits(WorkUnit* pPool, int numUnits);

    int submitFrame(Slice& slice, Picture* pic, EncSlice* pEncSlice = nullptr);
    int advanceFrame();

    int setPolicy(BatchPolicy ePolicy);
    int setWindowSize(int nTUs);

    BatchPolicy getPolicy() const;
    int getWindowSize() const;

    RingBuffer* getRingBuffer() { return m_pRing; }
    std::atomic<int8_t>* getCtuStates() { return m_pCtuStates; }

    virtual ~TUScheduler();

private:
    NoMallocThreadPool* m_pPool         = nullptr;
    RingBuffer*         m_pRing         = nullptr;
    WorkUnit*           m_pWorkPool     = nullptr;
    int                 m_poolSize      = 0;
    int                 m_windowSize    = 8;
    BatchPolicy         m_ePolicy       = BatchPolicy::STAGE_GLOBAL;
    bool                m_bInitialized  = false;

    std::atomic<int8_t>* m_pCtuStates   = nullptr;
    int                 m_numCtuInPic   = 0;
    int                 m_numCtuCols    = 0;
    bool                m_bFrameActive  = false;

    int xSubmitReady(WorkUnit* pUnits, int numUnits, int& completed);
    int xSubmitFrameReady();
    static void xOnComplete(WorkUnit* pWu, int& completed);
    int xCalcPoolSize(const MockTU* pTus, int numTus);
#ifdef VVENC_SOURCE
    int xCalcFramePoolSize(Slice& slice);
#endif
    int xInitCtuStates(Slice& slice);
};

}
