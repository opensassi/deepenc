/** \file     scheduler_test.cpp
    \brief    Unit tests for scheduler module classes (Phase 1)
 */

#include "source/Lib/Scheduler/WorkUnit.h"
#include "source/Lib/Scheduler/RingBuffer.h"
#include "source/Lib/Scheduler/TUPipelineDAG.h"
#include "source/Lib/Scheduler/TUScheduler.h"
#include "source/Lib/Scheduler/SchedulerTrace.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <cstdint>

int g_numTests = 0;
int g_numFails = 0;

#define TEST(x)   { int res = x; g_numTests++; g_numFails += res; }
#define TESTT(x,w){ int res = x; g_numTests++; g_numFails += res; }
#define ERROR(w)  { g_numTests++; g_numFails++; }

using namespace vvenc;

static int testWorkUnitStageEnum()
{
    if ((int)Stage::_COUNT != 23)
    {
        printf("FAIL: _COUNT expected 23, got %d\n", (int)Stage::_COUNT);
        return 1;
    }
    if ((int)Stage::INIT_PRED != 0) return 1;
    if ((int)Stage::PREDICT != 1) return 1;
    if ((int)Stage::RESIDUAL != 2) return 1;
    if ((int)Stage::FWD_XFORM != 3) return 1;
    if ((int)Stage::RECONSTRUCT != 10) return 1;
    if ((int)Stage::CTU_ENCODE != 12) return 1;
    if ((int)Stage::CCALF_RECON != 22) return 1;
    return 0;
}

static int testSpatialDepConstants()
{
    if (SPATIAL_LEFT != 1) return 1;
    if (SPATIAL_TOP != 2) return 1;
    if (SPATIAL_TOP_RIGHT != 4) return 1;
    if (SPATIAL_BOT_RIGHT != 8) return 1;
    if ((SPATIAL_LEFT | SPATIAL_TOP | SPATIAL_TOP_RIGHT | SPATIAL_BOT_RIGHT) != 15) return 1;
    return 0;
}

static int testWorkUnitDefaults()
{
    WorkUnit wu;
    if (wu.m_width != 0) return 1;
    if (wu.m_height != 0) return 1;
    if (wu.m_depCount.load() != 0) return 1;
    if (wu.m_pDependents != nullptr) return 1;
    if (wu.m_numDependents != 0) return 1;
    if (wu.m_pInputBuf != nullptr) return 1;
    if (wu.m_pOutputBuf != nullptr) return 1;
    if (wu.m_pfnExec != nullptr) return 1;
    if (wu.m_spatialDepMask != 0) return 1;
    return 0;
}

static int testWorkUnitDepCount()
{
    WorkUnit wu;
    wu.m_depCount.store(3, std::memory_order_relaxed);

    wu.m_depCount.fetch_sub(1, std::memory_order_acq_rel);
    if (wu.m_depCount.load() != 2) return 1;

    wu.m_depCount.fetch_sub(1, std::memory_order_acq_rel);
    if (wu.m_depCount.load() != 1) return 1;

    wu.m_depCount.fetch_sub(1, std::memory_order_acq_rel);
    if (wu.m_depCount.load() != 0) return 1;

    return 0;
}

static int testRingBufferInitInvalid()
{
    RingBuffer rb;
    if (rb.init(0, 8) != -1) return 1;
    if (rb.init(64, 0) != -1) return 1;
    rb.destroy();
    return 0;
}

static int testRingBufferAllocFree()
{
    RingBuffer rb;
    if (rb.init(64, 4) != 0) return 1;

    if (rb.getCapacity() != 4) return 1;
    if (rb.getFreeCount() != 4) return 1;

    void* s0 = rb.alloc();
    void* s1 = rb.alloc();
    void* s2 = rb.alloc();
    void* s3 = rb.alloc();

    if (!s0 || !s1 || !s2 || !s3) return 1;

    if (rb.getFreeCount() != 0) return 1;

    if (s0 == s1 || s0 == s2 || s0 == s3) return 1;
    if (s1 == s2 || s1 == s3) return 1;
    if (s2 == s3) return 1;

    void* s4 = rb.alloc();
    if (s4 != nullptr) return 1;

    rb.free(s0);
    if (rb.getFreeCount() != 1) return 1;

    void* s0again = rb.alloc();
    if (s0again == nullptr) return 1;

    rb.destroy();
    return 0;
}

static int testRingBufferWrapAround()
{
    RingBuffer rb;
    if (rb.init(64, 3) != 0) return 1;

    void* slots[3];
    for (int i = 0; i < 3; i++) slots[i] = rb.alloc();
    if (rb.alloc() != nullptr) return 1;

    for (int i = 0; i < 3; i++) rb.free(slots[i]);

    for (int i = 0; i < 3; i++)
    {
        void* s = rb.alloc();
        if (!s) return 1;
    }

    rb.destroy();
    return 0;
}

static int testRingBufferPtrRoundTrip()
{
    RingBuffer rb;
    if (rb.init(64, 2) != 0) return 1;

    void* s0 = rb.alloc();
    void* s1 = rb.alloc();
    if (!s0 || !s1) return 1;
    if (s0 == s1) return 1;

    rb.free(s0);
    rb.free(s1);

    if (rb.getFreeCount() != 2) return 1;

    void* s2 = rb.alloc();
    void* s3 = rb.alloc();
    if (!s2 || !s3) return 1;

    rb.destroy();
    return 0;
}

static int testDagSingleTu()
{
    MockTU tu = { 8, 8, 1, 0, 22 };
    int poolSize = TUPipelineDAG::estimatePoolSize(&tu, 1);
    if (poolSize != 7) return 1;

    WorkUnit* pool = new WorkUnit[poolSize];
    int numUnits = 0;
    int ret = TUPipelineDAG::build(&tu, 1, pool, poolSize, numUnits);
    if (ret != 0) return 1;
    if (numUnits != 7) return 1;

    delete[] pool;
    return 0;
}

static int testDagMultiTu()
{
    MockTU tus[2] = { { 8, 8, 1, 0, 22 }, { 4, 4, 1, 0, 27 } };
    int poolSize = TUPipelineDAG::estimatePoolSize(tus, 2);
    if (poolSize != 14) return 1;

    WorkUnit* pool = new WorkUnit[poolSize];
    int numUnits = 0;
    int ret = TUPipelineDAG::build(tus, 2, pool, poolSize, numUnits);
    if (ret != 0) return 1;
    if (numUnits != 14) return 1;

    delete[] pool;
    return 0;
}

static int testDagOverflow()
{
    MockTU tu = { 8, 8, 1, 0, 22 };
    WorkUnit pool[3];
    int numUnits = 0;
    int ret = TUPipelineDAG::build(&tu, 1, pool, 3, numUnits);
    if (ret != -1) return 1;
    return 0;
}

static int testDagMultiComponent()
{
    MockTU tu = { 8, 8, 7, 0, 22 };
    int poolSize = TUPipelineDAG::estimatePoolSize(&tu, 1);
    if (poolSize != 21) return 1;

    WorkUnit* pool = new WorkUnit[poolSize];
    int numUnits = 0;
    int ret = TUPipelineDAG::build(&tu, 1, pool, poolSize, numUnits);
    if (ret != 0) return 1;
    if (numUnits != 21) return 1;

    delete[] pool;
    return 0;
}

static int testSchedulerInitDestroy()
{
    TUScheduler sched;
    int ret = sched.init(nullptr, 8);
    if (ret != 0) return 1;
    sched.destroy();
    return 0;
}

static int testSchedulerInitInvalid()
{
    TUScheduler sched;
    int ret = sched.init(nullptr, 0);
    if (ret != -2) return 1;
    return 0;
}

static int testSchedulerSubmitModeTrial()
{
    TUScheduler sched;
    if (sched.init(nullptr, 8) != 0) return 1;

    MockTU tus[2] = { { 8, 8, 1, 0, 22 }, { 4, 4, 1, 0, 27 } };
    int ret = sched.submitModeTrial(tus, 2, nullptr, 0);
    if (ret != 0) return 1;

    sched.destroy();
    return 0;
}

static int testSchedulerSetPolicy()
{
    TUScheduler sched;
    if (sched.setPolicy(BatchPolicy::STAGE_GLOBAL) != 0) return 1;
    if (sched.setPolicy(BatchPolicy::TU_SEQUENTIAL) != 0) return 1;
    if (sched.setPolicy(BatchPolicy::HYBRID) != 0) return 1;

    BatchPolicy invalid = (BatchPolicy)99;
    if (sched.setPolicy(invalid) != -1) return 1;

    return 0;
}

static int testSchedulerSetWindowSize()
{
    TUScheduler sched;
    if (sched.setWindowSize(4) != 0) return 1;
    if (sched.getWindowSize() != 4) return 1;
    if (sched.setWindowSize(0) != -1) return 1;
    return 0;
}

static int testSchedulerSubmitBeforeInit()
{
    TUScheduler sched;
    MockTU tu = { 8, 8, 1, 0, 22 };
    int ret = sched.submitModeTrial(&tu, 1, nullptr, 0);
    if (ret != -1) return 1;
    return 0;
}

static int testSchedulerStageGlobal()
{
    TUScheduler sched;
    sched.init(nullptr, 8);
    sched.setPolicy(BatchPolicy::STAGE_GLOBAL);

    MockTU tus[3] = { { 8, 8, 1, 0, 22 }, { 8, 8, 1, 0, 22 }, { 8, 8, 1, 0, 22 } };
    int ret = sched.submitModeTrial(tus, 3, nullptr, 0);
    if (ret != 0) return 1;

    sched.destroy();
    return 0;
}

static int testSchedulerTuSequential()
{
    TUScheduler sched;
    sched.init(nullptr, 8);
    sched.setPolicy(BatchPolicy::TU_SEQUENTIAL);

    MockTU tus[3] = { { 8, 8, 1, 0, 22 }, { 4, 4, 1, 0, 27 }, { 8, 8, 7, 0, 22 } };
    int ret = sched.submitModeTrial(tus, 3, nullptr, 0);
    if (ret != 0) return 1;

    sched.destroy();
    return 0;
}

static int testTraceInitDestroy()
{
    SchedulerTrace trace;
    int ret = trace.init("/tmp/sched_test_trace.bin");
    if (ret != 0) return 1;
    trace.destroy();
    std::remove("/tmp/sched_test_trace.bin");
    return 0;
}

static int testTraceInitNull()
{
    SchedulerTrace trace;
    int ret = trace.init(nullptr);
    if (ret != -1) return 1;
    return 0;
}

static int testTraceSingleRecord()
{
    SchedulerTrace trace;
    if (trace.init("/tmp/sched_test_stage.bin") != 0) return 1;

    WorkUnit wu;
    wu.m_eStage = Stage::INIT_PRED;
    wu.m_tuId = 0;
    wu.m_compId = 0;
    wu.m_qp = 22;
    wu.m_width = 8;
    wu.m_height = 8;
    wu.m_mtsIdx = 0;

    uint8_t inputData[16] = { 0 };
    int ret = trace.recordStage(&wu, inputData, 16);
    if (ret != 0) return 1;
    if (trace.getStageCount() != 1) return 1;

    trace.destroy();
    std::remove("/tmp/sched_test_stage.bin");
    return 0;
}

static int testWorkUnitContextPtr()
{
    WorkUnit wu;
    if (wu.m_pCtx != nullptr) return 1;
    int value = 42;
    wu.m_pCtx = &value;
    if (*(int*)wu.m_pCtx != 42) return 1;
    wu.m_pCtx = nullptr;
    return 0;
}

static int testTraceMultipleRecords()
{
    SchedulerTrace trace;
    if (trace.init("/tmp/sched_test_multi.bin") != 0) return 1;

    for (int i = 0; i < 10; i++)
    {
        WorkUnit wu;
        wu.m_eStage = (Stage)(i % 7);
        wu.m_tuId = (uint32_t)i;
        wu.m_compId = 0;
        wu.m_qp = 22;
        wu.m_width = 8;
        wu.m_height = 8;
        wu.m_mtsIdx = 0;
        trace.recordStage(&wu, nullptr, 0);
    }

    if (trace.getStageCount() != 10) return 1;
    trace.destroy();
    std::remove("/tmp/sched_test_multi.bin");
    return 0;
}

int main(int argc, char* argv[])
{
    int testId = 0;
    if (argc > 1) testId = atoi(argv[1]);

    switch (testId)
    {
    case 0:
        TESTT(testWorkUnitStageEnum(), "WorkUnit Stage enum");
        TESTT(testSpatialDepConstants(), "SpatialDepType constants");
        TESTT(testWorkUnitDefaults(), "WorkUnit default state");
        TESTT(testWorkUnitDepCount(), "WorkUnit depCount decrement");
        TESTT(testRingBufferInitInvalid(), "RingBuffer init invalid params");
        TESTT(testRingBufferAllocFree(), "RingBuffer alloc/free cycle");
        TESTT(testRingBufferWrapAround(), "RingBuffer wrap-around");
        TESTT(testRingBufferPtrRoundTrip(), "RingBuffer pointer round-trip");
        TESTT(testDagSingleTu(), "DAG single TU luma");
        TESTT(testDagMultiTu(), "DAG multi TU");
        TESTT(testDagOverflow(), "DAG pool overflow");
        TESTT(testDagMultiComponent(), "DAG multi component");
        TESTT(testSchedulerInitDestroy(), "Scheduler init/destroy");
        TESTT(testSchedulerInitInvalid(), "Scheduler init invalid");
        TESTT(testSchedulerSubmitModeTrial(), "Scheduler submitModeTrial");
        TESTT(testSchedulerSetPolicy(), "Scheduler setPolicy");
        TESTT(testSchedulerSetWindowSize(), "Scheduler setWindowSize");
        TESTT(testSchedulerSubmitBeforeInit(), "Scheduler submit before init");
        TESTT(testSchedulerStageGlobal(), "Scheduler STAGE_GLOBAL");
        TESTT(testSchedulerTuSequential(), "Scheduler TU_SEQUENTIAL");
        TESTT(testTraceInitDestroy(), "Trace init/destroy");
        TESTT(testTraceInitNull(), "Trace init null filename");
        TESTT(testTraceSingleRecord(), "Trace single record");
        TESTT(testTraceMultipleRecords(), "Trace multiple records");
        TESTT(testWorkUnitContextPtr(), "WorkUnit context pointer");
        printf("SCHEDULER_TEST: %d tests, %d fails\n", g_numTests, g_numFails);
        break;

    case 1:  return testWorkUnitStageEnum();
    case 2:  return testSpatialDepConstants();
    case 3:  return testWorkUnitDefaults();
    case 4:  return testWorkUnitDepCount();
    case 5:  return testRingBufferInitInvalid();
    case 6:  return testRingBufferAllocFree();
    case 7:  return testRingBufferWrapAround();
    case 8:  return testRingBufferPtrRoundTrip();
    case 9:  return testDagSingleTu();
    case 10: return testDagMultiTu();
    case 11: return testDagOverflow();
    case 12: return testDagMultiComponent();
    case 13: return testSchedulerInitDestroy();
    case 14: return testSchedulerInitInvalid();
    case 15: return testSchedulerSubmitModeTrial();
    case 16: return testSchedulerSetPolicy();
    case 17: return testSchedulerSetWindowSize();
    case 18: return testSchedulerSubmitBeforeInit();
    case 19: return testSchedulerStageGlobal();
    case 20: return testSchedulerTuSequential();
    case 21: return testTraceInitDestroy();
    case 22: return testTraceInitNull();
    case 23: return testTraceSingleRecord();
    case 24: return testTraceMultipleRecords();
    case 25: return testWorkUnitContextPtr();

    default:
        printf("SCHEDULER_TEST: unknown test id %d\n", testId);
        return 1;
    }

    return g_numFails > 0 ? 1 : 0;
}
