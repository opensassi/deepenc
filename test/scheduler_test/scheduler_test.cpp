#include "Scheduler/WorkUnit.h"
#include "Scheduler/TUPipelineDAG.h"
#include "Scheduler/TUScheduler.h"
#include "Scheduler/RingBuffer.h"
#include "Scheduler/SchedulerTrace.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <vector>

static int g_numTests  = 0;
static int g_numFails  = 0;

#define TEST(x)   { int res = (x); g_numTests++; if (res) { g_numFails++; fprintf(stderr, "FAIL %s:%d: test %d\n", __FILE__, __LINE__, res); } }
#define CHECK(x)  { if (!(x)) { g_numFails++; fprintf(stderr, "FAIL %s:%d: check(%s)\n", __FILE__, __LINE__, #x); } else g_numTests++; }

static int testWorkUnitBasics()
{
    vvenc::WorkUnit wu;
    if (wu.m_depCount.load() != 0) return 1;
    if (wu.m_pDependents != nullptr) return 2;
    if (wu.m_numDependents != 0) return 3;
    if (wu.m_pCtx != nullptr) return 4;
    if (wu.m_pfnExec != nullptr) return 5;

    wu.m_depCount.store(5);
    if (wu.m_depCount.load() != 5) return 8;
    wu.m_depCount.fetch_sub(1);
    if (wu.m_depCount.load() != 4) return 9;

    if ((int)vvenc::Stage::_COUNT != 23) return 10;

    return 0;
}

static int testRingBufferInit()
{
    vvenc::RingBuffer rb;
    if (rb.init(0, 8) == 0) return 1;
    if (rb.init(64, 0) == 0) return 2;
    if (rb.init(64, 8) != 0) return 3;
    if (rb.getCapacity() != 8) return 4;
    if (rb.getFreeCount() != 8) return 5;
    rb.destroy();
    return 0;
}

static int testRingBufferAllocFree()
{
    vvenc::RingBuffer rb;
    if (rb.init(64, 4) != 0) return 1;

    void* slots[4];
    for (int i = 0; i < 4; i++)
    {
        slots[i] = rb.alloc();
        if (!slots[i]) return 10 + i;
    }
    if (rb.getFreeCount() != 0) return 20;

    for (int i = 0; i < 4; i++)
    {
        if (rb.free(slots[i]) != 0) return 30 + i;
    }
    if (rb.getFreeCount() != 4) return 40;

    rb.destroy();
    return 0;
}

static int testRingBufferExhaustion()
{
    vvenc::RingBuffer rb;
    rb.init(64, 2);
    void* a = rb.alloc();
    void* b = rb.alloc();
    if (rb.alloc() != nullptr) return 1;
    rb.free(a);
    if (rb.alloc() == nullptr) return 2;
    rb.free(b);
    rb.destroy();
    return 0;
}

static int testTUPipelineDAG_3Mock()
{
    vvenc::MockTU tus[3];
    tus[0] = { 8, 8, 0x01, 0, 32 };
    tus[1] = { 8, 8, 0x02, 0, 32 };
    tus[2] = { 4, 4, 0x04, 0, 32 };

    int poolSize = vvenc::TUPipelineDAG::estimatePoolSize(tus, 3);
    int expected = 3 * 7;
    if (poolSize < expected) return 1;

    std::vector<vvenc::WorkUnit> pool(poolSize);
    int numUnits = 0;
    int ret = vvenc::TUPipelineDAG::build(tus, 3, pool.data(), poolSize, numUnits);
    if (ret != 0) return 2;
    if (numUnits != expected) return 3;

    int seenTuId0 = 0, seenTuId1 = 0, seenTuId2 = 0;
    for (int i = 0; i < numUnits; i++)
    {
        if (pool[i].m_tuId == 0) seenTuId0++;
        if (pool[i].m_tuId == 1) seenTuId1++;
        if (pool[i].m_tuId == 2) seenTuId2++;
        if (pool[i].m_width <= 0 || pool[i].m_height <= 0) return 10 + i;
    }
    if (seenTuId0 != 7) return 20;
    if (seenTuId1 != 7) return 21;
    if (seenTuId2 != 7) return 22;

    int depCountSum = 0;
    for (int i = 0; i < numUnits; i++)
        depCountSum += pool[i].m_depCount.load();
    if (depCountSum != numUnits - 3) return 30;

    return 0;
}

struct TestExecCtx
{
    int touchCount;
};

static bool testExecFunc(vvenc::WorkUnit* pWu, void* pScratch)
{
    (void)pScratch;
    ((TestExecCtx*)pWu->m_pScratch)->touchCount++;
    return true;
}

static int testTUSchedulerInit()
{
    vvenc::TUScheduler sched;
    if (sched.init(nullptr, 0) == 0) return 1;
    if (sched.init(nullptr, 8) != 0) return 2;
    if (sched.getWindowSize() != 8) return 3;
    if (sched.getPolicy() != vvenc::BatchPolicy::STAGE_GLOBAL) return 4;
    sched.destroy();
    return 0;
}

static int testTUSchedulerSubmitModeTrial()
{
    vvenc::TUScheduler sched;
    if (sched.init(nullptr, 8) != 0) return 1;

    vvenc::MockTU tus[2];
    tus[0] = { 8, 8, 0x01, 0, 32 };
    tus[1] = { 8, 8, 0x01, 0, 32 };

    int poolSize = vvenc::TUPipelineDAG::estimatePoolSize(tus, 2);
    std::vector<vvenc::WorkUnit> pool(poolSize);
    int numUnits = 0;
    vvenc::TUPipelineDAG::build(tus, 2, pool.data(), poolSize, numUnits);

    TestExecCtx ctx = { 0 };
    for (int i = 0; i < numUnits; i++)
    {
        pool[i].m_pfnExec = testExecFunc;
        pool[i].m_pScratch = &ctx;
    }

    int ret = sched.executeWorkUnits(pool.data(), numUnits);
    if (ret != 0) return 2;
    if (ctx.touchCount != numUnits)
    {
        return 3;
    }

    sched.destroy();
    return 0;
}

static int testTUSchedulerOrdering()
{
    vvenc::TUScheduler sched;
    sched.init(nullptr, 8);

    vvenc::MockTU tus[1];
    tus[0] = { 8, 8, 0x01, 0, 32 };

    int poolSize = vvenc::TUPipelineDAG::estimatePoolSize(tus, 1);
    std::vector<vvenc::WorkUnit> pool(poolSize);
    int numUnits = 0;
    vvenc::TUPipelineDAG::build(tus, 1, pool.data(), poolSize, numUnits);

    for (int i = 0; i < numUnits; i++)
    {
        pool[i].m_pfnExec = testExecFunc;
    }

    TestExecCtx ctx = { 0 };
    for (int i = 0; i < numUnits; i++)
    {
        pool[i].m_pScratch = &ctx;
    }

    int ret = sched.executeWorkUnits(pool.data(), numUnits);
    if (ret != 0) return 1;

    vvenc::Stage expectedOrder[7] = {
        vvenc::Stage::INIT_PRED,
        vvenc::Stage::PREDICT,
        vvenc::Stage::RESIDUAL,
        vvenc::Stage::FWD_XFORM,
        vvenc::Stage::QUANT_FILL,
        vvenc::Stage::INV_XFORM,
        vvenc::Stage::RECONSTRUCT
    };

    for (int i = 0; i < 7; i++)
    {
        if (pool[i].m_eStage != expectedOrder[i]) return 10 + i;
    }

    sched.destroy();
    return 0;
}

static int testTUSchedulerPolicy()
{
    vvenc::TUScheduler sched;
    sched.init(nullptr, 8);

    if (sched.getPolicy() != vvenc::BatchPolicy::STAGE_GLOBAL) return 1;
    if (sched.setPolicy(vvenc::BatchPolicy::TU_SEQUENTIAL) != 0) return 2;
    if (sched.getPolicy() != vvenc::BatchPolicy::TU_SEQUENTIAL) return 3;
    if (sched.setPolicy(vvenc::BatchPolicy::WAVEFRONT) != 0) return 4;
    if (sched.getPolicy() != vvenc::BatchPolicy::WAVEFRONT) return 5;
    if (sched.setPolicy(vvenc::BatchPolicy::HYBRID) != 0) return 6;
    if (sched.getPolicy() != vvenc::BatchPolicy::HYBRID) return 7;

    if (sched.setWindowSize(0) == 0) return 8;
    if (sched.setWindowSize(16) != 0) return 9;
    if (sched.getWindowSize() != 16) return 10;

    sched.destroy();
    return 0;
}

static std::atomic<int> g_execCounter{0};

static bool tuSeqExecFunc(vvenc::WorkUnit* pWu, void*)
{
    pWu->m_pScratch = (void*)(size_t)g_execCounter.fetch_add(1);
    return true;
}

static int testTUSchedulerTuSequential()
{
    g_execCounter.store(0);

    vvenc::TUScheduler sched;
    sched.init(nullptr, 8);
    sched.setPolicy(vvenc::BatchPolicy::TU_SEQUENTIAL);

    vvenc::MockTU tus[2];
    tus[0] = { 8, 8, 0x01, 0, 32 };
    tus[1] = { 8, 8, 0x01, 0, 32 };

    int poolSize = vvenc::TUPipelineDAG::estimatePoolSize(tus, 2);
    std::vector<vvenc::WorkUnit> pool(poolSize);
    int numUnits = 0;
    vvenc::TUPipelineDAG::build(tus, 2, pool.data(), poolSize, numUnits);

    for (int i = 0; i < numUnits; i++)
        pool[i].m_pfnExec = tuSeqExecFunc;

    int ret = sched.executeWorkUnits(pool.data(), numUnits);
    if (ret != 0) return 1;

    int lastTu0Order = -1;
    int firstTu1Order = 999;
    for (int i = 0; i < numUnits; i++)
    {
        int order = (int)(size_t)pool[i].m_pScratch;
        if (pool[i].m_tuId == 0 && order > lastTu0Order)
            lastTu0Order = order;
        if (pool[i].m_tuId == 1 && order < firstTu1Order)
            firstTu1Order = order;
    }

    if (lastTu0Order < 0 || firstTu1Order > 27) return 2;
    if (firstTu1Order <= lastTu0Order) return 3;

    sched.destroy();
    return 0;
}

static int testTUSchedulerInvalid()
{
    vvenc::TUScheduler sched;

    vvenc::MockTU tu = { 8, 8, 0x01, 0, 32 };
    if (sched.submitModeTrial(&tu, 1, nullptr, 0) == 0) return 2;

    sched.init(nullptr, 8);
    int poolSize = vvenc::TUPipelineDAG::estimatePoolSize(&tu, 1);
    std::vector<vvenc::WorkUnit> pool(poolSize);
    if (sched.executeWorkUnits(nullptr, 0) == 0) return 4;
    if (sched.executeWorkUnits(pool.data(), 0) == 0) return 5;

    sched.destroy();
    return 0;
}

static int testSchedulerTraceInit()
{
    vvenc::SchedulerTrace trace;
    if (trace.init(nullptr) == 0) return 1;
    if (trace.getStageCount() != 0) return 2;
    if (trace.init("/tmp/sched_trace_test.bin") != 0) return 3;
    if (trace.getStageCount() != 0) return 4;
    trace.destroy();

    FILE* f = fopen("/tmp/sched_trace_test.bin", "rb");
    if (!f) return 5;
    uint32_t header[2];
    size_t nr = fread(header, 8, 1, f);
    if (nr != 1) { fclose(f); return 6; }
    if (header[0] != 0x53434854) { fclose(f); return 7; }
    if (header[1] != 1) { fclose(f); return 8; }
    uint8_t endRecord[8];
    nr = fread(endRecord, 8, 1, f);
    if (nr != 1) { fclose(f); return 9; }
    if (endRecord[0] != 3) { fclose(f); return 10; }
    fclose(f);
    remove("/tmp/sched_trace_test.bin");
    return 0;
}

static int testSchedulerTraceRecord()
{
    {
        vvenc::SchedulerTrace trace;
        if (trace.init("/tmp/sched_trace_rec.bin") != 0) return 1;

        uint8_t inputData[16] = { 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 };
        trace.recordStageRaw(0, 0, 0, 32, 8, 8, 0, inputData, 16);
        trace.recordStageRaw(0, 1, 0, 32, 8, 8, 0, inputData, 8);
        trace.recordStageRaw(1, 0, 1, 32, 4, 4, 0, nullptr, 0);
        trace.recordFrameMarker(0, 10);
    }

    FILE* f = fopen("/tmp/sched_trace_rec.bin", "rb");
    if (!f) return 2;
    uint32_t magic, ver;
    size_t nr = fread(&magic, 4, 1, f);
    nr += fread(&ver, 4, 1, f);
    if (nr != 2 || magic != 0x53434854 || ver != 1) { fclose(f); return 3; }

    int recordCount = 0;
    uint8_t hdr[8];
    while (fread(hdr, 8, 1, f) == 1)
    {
        uint8_t type = hdr[0];
        uint32_t payloadSize = 0;
        memcpy(&payloadSize, hdr + 4, 4);
        if (type == 3) break;
        recordCount++;
        if (payloadSize > 0)
            fseek(f, payloadSize, SEEK_CUR);
    }
    fclose(f);
    if (recordCount != 4) return 4;

    remove("/tmp/sched_trace_rec.bin");
    return 0;
}

int main(int argc, char* argv[])
{
    (void)argc; (void)argv;


    TEST(testWorkUnitBasics());
    TEST(testRingBufferInit());
    TEST(testRingBufferAllocFree());
    TEST(testRingBufferExhaustion());
    TEST(testTUPipelineDAG_3Mock());
    TEST(testTUSchedulerInit());
    TEST(testTUSchedulerSubmitModeTrial());
    TEST(testTUSchedulerOrdering());
    TEST(testTUSchedulerPolicy());
    TEST(testTUSchedulerTuSequential());
    TEST(testTUSchedulerInvalid());
    TEST(testSchedulerTraceInit());
    TEST(testSchedulerTraceRecord());

    fprintf(stdout, "scheduler_test: %d tests, %d failures\n", g_numTests, g_numFails);
    return g_numFails > 0 ? 1 : 0;
}
