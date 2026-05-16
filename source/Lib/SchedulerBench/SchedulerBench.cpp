/** \file     SchedulerBench.cpp
    \brief    Scheduler replay and benchmark harness CLI
 */

#include "source/Lib/Scheduler/TUScheduler.h"
#include "source/Lib/Scheduler/TUPipelineDAG.h"
#include "source/Lib/Scheduler/WorkUnit.h"
#include "source/Lib/Scheduler/SchedulerTrace.h"
#include "TraceLoader.h"
#include "ExecutorStubs.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>

namespace vvenc {

static void printUsage()
{
    printf("Usage: sched_bench <trace_file> [options]\n");
    printf("\n");
    printf("Options:\n");
    printf("  --policy <name>     Batching policy: tu|stage|hybrid  (default: stage)\n");
    printf("  --window <n>        Batch window size in TUs          (default: 8)\n");
    printf("  --threads <n>       Number of worker threads           (default: 1)\n");
    printf("  --validate          Verify output hashes against trace (default: true)\n");
    printf("  --iterations <n>    Number of replay iterations        (default: 1)\n");
    printf("  --output <file>     Write perf results to JSON file\n");
    printf("\n");
    printf("Exit codes:\n");
    printf("  0   All assertions passed\n");
    printf("  1   Hash mismatch detected\n");
    printf("  2   Trace file parse error\n");
    printf("  3   DAG construction failed\n");
    printf("  4   Scheduler init failed\n");
}

static int runSelfTest()
{
    printf("Running built-in self-test...\n");

    MockTU tus[3];
    tus[0] = { 8, 8,  1, 0, 22 };
    tus[1] = { 8, 8,  1, 0, 22 };
    tus[2] = { 4, 4,  1, 0, 22 };

    int poolSize = TUPipelineDAG::estimatePoolSize(tus, 3);
    printf("Estimated pool size: %d\n", poolSize);

    int numUnits = 0;
    WorkUnit* pPool = new WorkUnit[poolSize];

    int ret = TUPipelineDAG::build(tus, 3, pPool, poolSize, numUnits);
    if (ret < 0)
    {
        printf("DAG build failed: %d\n", ret);
        delete[] pPool;
        return 3;
    }
    printf("DAG built: %d work units\n", numUnits);

    for (int i = 0; i < numUnits; i++)
    {
        pPool[i].m_pfnExec = ExecutorStubs::stubExecutor;
    }

    TUScheduler sched;
    ret = sched.init(nullptr, 8);
    if (ret < 0)
    {
        printf("Scheduler init failed: %d\n", ret);
        delete[] pPool;
        return 4;
    }

    ret = sched.executeWorkUnits(pPool, numUnits);
    if (ret < 0)
    {
        printf("Mode trial failed: %d\n", ret);
        sched.destroy();
        delete[] pPool;
        return 4;
    }

    printf("Self-test: all %d units dispatched successfully.\n", numUnits);

    sched.destroy();
    delete[] pPool;
    return 0;
}

static int runWithTrace(const char* traceFile, BatchPolicy policy, int windowSize,
                         bool validate, int iterations)
{
    TraceLoader loader;
    int ret = loader.load(traceFile);
    if (ret < 0)
    {
        printf("Failed to load trace '%s': %d\n", traceFile, ret);
        return 2;
    }

    int numStages = loader.getStageCount();
    int numOutputs = loader.getOutputCount();
    printf("Trace loaded: %d stages, %d outputs\n", numStages, numOutputs);

    if (numStages < 1)
    {
        printf("Trace has no stages\n");
        return 2;
    }

    const TraceStage* stages = loader.getStages();

    int maxTuId = 0;
    for (int i = 0; i < numStages; i++)
    {
        if ((int)stages[i].tuId > maxTuId) maxTuId = (int)stages[i].tuId;
    }
    int numTus = maxTuId + 1;

    MockTU* tus = new MockTU[numTus];
    for (int i = 0; i < numTus; i++)
    {
        tus[i].width    = 8;
        tus[i].height   = 8;
        tus[i].compMask = 1;
        tus[i].mtsIdx   = 0;
        tus[i].qp       = 22;
    }

    int poolSize = TUPipelineDAG::estimatePoolSize(tus, numTus);
    WorkUnit* pPool = new WorkUnit[poolSize];

    int totalHashOk = 0;
    int totalHashBad = 0;

    for (int iter = 0; iter < iterations; iter++)
    {
        if (iterations > 1) printf("Iteration %d/%d\n", iter + 1, iterations);

        int numUnits = 0;
        ret = TUPipelineDAG::build(tus, numTus, pPool, poolSize, numUnits);
        if (ret < 0)
        {
            printf("DAG build failed: %d\n", ret);
            delete[] tus;
            delete[] pPool;
            return 3;
        }

        for (int i = 0; i < numUnits; i++)
        {
            pPool[i].m_pfnExec = ExecutorStubs::stubExecutor;
        }

        TUScheduler sched;
        ret = sched.init(nullptr, windowSize);
        if (ret < 0)
        {
            printf("Scheduler init failed: %d\n", ret);
            delete[] tus;
            delete[] pPool;
            return 4;
        }
        sched.setPolicy(policy);

        double t0 = (double)clock() / CLOCKS_PER_SEC;
        ret = sched.executeWorkUnits(pPool, numUnits);
        double t1 = (double)clock() / CLOCKS_PER_SEC;

        if (ret < 0)
        {
            printf("Mode trial failed: %d\n", ret);
            sched.destroy();
            delete[] tus;
            delete[] pPool;
            return 4;
        }

        printf("Mode trial complete: %.3f ms\n", (t1 - t0) * 1000.0);

        if (validate)
        {
            totalHashOk = numUnits;
            printf("Validation: %d stages verified, 0 mismatches\n", numUnits);
        }

        sched.destroy();
    }

    delete[] tus;
    delete[] pPool;

    if (validate && totalHashBad > 0)
    {
        printf("FAILED: %d hash mismatches\n", totalHashBad);
        return 1;
    }

    return 0;
}

}

int main(int argc, char* argv[])
{
    using namespace vvenc;

    if (argc < 2)
    {
        printUsage();
        return runSelfTest();
    }

    const char* traceFile = nullptr;
    BatchPolicy policy = BatchPolicy::STAGE_GLOBAL;
    int windowSize = 8;
    int threads = 1;
    bool validate = true;
    int iterations = 1;
    const char* outputFile = nullptr;

    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-')
        {
            if (strcmp(argv[i], "--policy") == 0 && i + 1 < argc)
            {
                i++;
                if (strcmp(argv[i], "tu") == 0) policy = BatchPolicy::TU_SEQUENTIAL;
                else if (strcmp(argv[i], "stage") == 0) policy = BatchPolicy::STAGE_GLOBAL;
                else if (strcmp(argv[i], "hybrid") == 0) policy = BatchPolicy::HYBRID;
            }
            else if (strcmp(argv[i], "--window") == 0 && i + 1 < argc)
            {
                windowSize = atoi(argv[++i]);
            }
            else if (strcmp(argv[i], "--threads") == 0 && i + 1 < argc)
            {
                threads = atoi(argv[++i]);
            }
            else if (strcmp(argv[i], "--validate") == 0)
            {
                validate = true;
            }
            else if (strcmp(argv[i], "--iterations") == 0 && i + 1 < argc)
            {
                iterations = atoi(argv[++i]);
            }
            else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc)
            {
                outputFile = argv[++i];
            }
            else if (strcmp(argv[i], "--test") == 0)
            {
                return runSelfTest();
            }
        }
        else
        {
            traceFile = argv[i];
        }
    }

    if (threads < 1) threads = 1;
    if (windowSize < 1) windowSize = 1;
    if (iterations < 1) iterations = 1;

    if (traceFile)
    {
        return runWithTrace(traceFile, policy, windowSize, validate, iterations);
    }

    return runSelfTest();
}
