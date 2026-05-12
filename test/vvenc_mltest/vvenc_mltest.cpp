/** \file     vvenc_mltest.cpp
    \brief    Unit tests for MLTools module (FASTSplitPredictor, CUFeatureExtractor, FakeModelFactory)
*/

#include "MLTools/FASTSplitPredictor.h"
#include "MLTools/CUFeatureExtractor.h"
#include "MLTools/FakeModelFactory.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>

#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

int  g_numTests = 0;
int  g_numFails = 0;

#define TEST(x)   { int res = x; g_numTests++; g_numFails += res; }
#define TESTT(x,w){ int res = x; g_numTests++; g_numFails += res; }
#define ERROR(w)  { g_numTests++; g_numFails++; }

static const char* TEST_DIR = "test_models";

// ====================================================================================================================
// FakeModelFactory tests
// ====================================================================================================================

static int testFakeModelFactory_writeSingle()
{
    const std::string path = std::string(TEST_DIR) + "/test_model.txt";
    int ret = vvenc::FakeModelFactory::writeDummyModel(path, 22, 0.5);
    if (ret != 0)
    {
        std::cerr << "FAIL: writeDummyModel returned " << ret << std::endl;
        return 1;
    }

    FILE* f = fopen(path.c_str(), "r");
    if (!f)
    {
        std::cerr << "FAIL: model file not found after write" << std::endl;
        return 1;
    }

    char buf[256];
    bool foundLeaf = false;
    bool foundVersion = false;
    while (fgets(buf, sizeof(buf), f))
    {
        if (strstr(buf, "leaf_value="))
            foundLeaf = true;
        if (strstr(buf, "version=v4"))
            foundVersion = true;
    }
    fclose(f);

    if (!foundLeaf)
    {
        std::cerr << "FAIL: model file missing leaf_value" << std::endl;
        return 1;
    }
    if (!foundVersion)
    {
        std::cerr << "FAIL: model file missing version=v4 header" << std::endl;
        return 1;
    }

    std::remove(path.c_str());
    return 0;
}

static int testFakeModelFactory_writeAll()
{
    int ret = vvenc::FakeModelFactory::writeAllDummyModels(TEST_DIR, 0.5);
    if (ret != 0)
    {
        std::cerr << "FAIL: writeAllDummyModels returned " << ret << std::endl;
        return 1;
    }

    for (int i = 0; i < 5; ++i)
    {
        std::string path = std::string(TEST_DIR) + "/" + vvenc::FakeModelFactory::MODEL_NAMES[i];
        FILE* f = fopen(path.c_str(), "r");
        if (!f)
        {
            std::cerr << "FAIL: " << path << " not found" << std::endl;
            return 1;
        }
        fclose(f);
        std::remove(path.c_str());
    }

    return 0;
}

// ====================================================================================================================
// FASTSplitPredictor tests (requires dummy model files)
// ====================================================================================================================

static int testFASTSplitPredictor_loadAndPredict()
{
    vvenc::FakeModelFactory::writeAllDummyModels(TEST_DIR, 0.5);

    vvenc::FASTSplitPredictor predictor;
    int ret = predictor.init(TEST_DIR);
    if (ret != 0)
    {
        std::cerr << "FAIL: init returned " << ret << std::endl;
        return 1;
    }

    if (!predictor.isInitialized())
    {
        std::cerr << "FAIL: isInitialized false after successful init" << std::endl;
        return 1;
    }

    std::vector<double> features(22, 0.0);
    vvenc::FASTSplitPredictor::SplitType split;
    double confidence = 0.0;

    ret = predictor.predict(features, 0.3, split, confidence);
    if (ret != 0)
    {
        std::cerr << "FAIL: predict returned " << ret << std::endl;
        return 1;
    }

    // Dummy models have leaf_value=0.0 → sigmoid(0.0) = 0.5
    // Threshold 0.3 < 0.5 → should return a split
    if (split == vvenc::FASTSplitPredictor::NO_SPLIT)
    {
        std::cerr << "FAIL: expected non-NO_SPLIT for conf=0.5 >= thresh=0.3, got NO_SPLIT" << std::endl;
        return 1;
    }

    // sigmoid(0.0) = 0.5 exactly
    if (confidence < 0.49 || confidence > 0.51)
    {
        std::cerr << "FAIL: expected confidence ~0.5, got " << confidence << std::endl;
        return 1;
    }

    predictor.release();
    return 0;
}

static int testFASTSplitPredictor_belowThreshold()
{
    vvenc::FASTSplitPredictor predictor;
    int ret = predictor.init(TEST_DIR);
    if (ret != 0)
    {
        vvenc::FakeModelFactory::writeAllDummyModels(TEST_DIR, 0.5);
        ret = predictor.init(TEST_DIR);
        if (ret != 0)
        {
            std::cerr << "FAIL: init returned " << ret << std::endl;
            return 1;
        }
    }

    std::vector<double> features(22, 0.0);
    vvenc::FASTSplitPredictor::SplitType split;
    double confidence = 0.0;

    // threshold 0.9 > model output 0.5 → should return NO_SPLIT
    ret = predictor.predict(features, 0.9, split, confidence);
    if (ret != 0)
    {
        std::cerr << "FAIL: predict returned " << ret << std::endl;
        return 1;
    }

    if (split != vvenc::FASTSplitPredictor::NO_SPLIT)
    {
        std::cerr << "FAIL: expected NO_SPLIT for conf=0.5 < thresh=0.9" << std::endl;
        return 1;
    }

    predictor.release();
    return 0;
}

static int testFASTSplitPredictor_predictWithoutInit()
{
    vvenc::FASTSplitPredictor predictor;

    std::vector<double> features(22, 0.0);
    vvenc::FASTSplitPredictor::SplitType split;
    double confidence = 0.0;

    int ret = predictor.predict(features, 0.5, split, confidence);
    if (ret == 0)
    {
        std::cerr << "FAIL: expected error for predict() without init()" << std::endl;
        return 1;
    }

    return 0;
}

static int testFASTSplitPredictor_doubleInit()
{
    vvenc::FASTSplitPredictor predictor;
    predictor.init(TEST_DIR);

    int ret = predictor.init(TEST_DIR);
    if (ret == 0)
    {
        std::cerr << "FAIL: expected error for double init()" << std::endl;
        predictor.release();
        return 1;
    }

    predictor.release();
    return 0;
}

static int testFASTSplitPredictor_releaseCycle()
{
    vvenc::FASTSplitPredictor predictor;
    predictor.init(TEST_DIR);
    int ret = predictor.release();
    if (ret != 0)
    {
        std::cerr << "FAIL: release returned " << ret << std::endl;
        return 1;
    }

    if (predictor.isInitialized())
    {
        std::cerr << "FAIL: isInitialized true after release" << std::endl;
        return 1;
    }

    return 0;
}

static int testFASTSplitPredictor_noModelsDir()
{
    vvenc::FASTSplitPredictor predictor;
    int ret = predictor.init("/nonexistent/path");
    if (ret == 0)
    {
        std::cerr << "FAIL: expected error for init() with invalid dir" << std::endl;
        return 1;
    }

    return 0;
}

// ====================================================================================================================
// CUFeatureExtractor tests
// ====================================================================================================================

static int testCUFeatureExtractor_defaultFeatures()
{
    // We can't easily construct a real CodingUnit without the full encoder context,
    // but we can verify the static feature count constant.
    if (vvenc::CUFeatureExtractor::NUM_FEATURES != 22)
    {
        std::cerr << "FAIL: expected NUM_FEATURES=22, got "
                  << vvenc::CUFeatureExtractor::NUM_FEATURES << std::endl;
        return 1;
    }

    return 0;
}

// ====================================================================================================================
// Main
// ====================================================================================================================

int main(int argc, char* argv[])
{
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    // Create temp test directory
    mkdir(TEST_DIR, 0755);

    int testId = 0;
    if (argc > 1)
        testId = atoi(argv[1]);

    bool runAll = (testId == 0);

    struct TestEntry
    {
        int   id;
        int (*func)();
        const char* name;
    };

    TestEntry tests[] =
    {
        {  1, testFakeModelFactory_writeSingle,            "FakeModelFactory_writeSingle" },
        {  2, testFakeModelFactory_writeAll,               "FakeModelFactory_writeAll" },
        {  3, testFASTSplitPredictor_loadAndPredict,       "FASTSplitPredictor_loadAndPredict" },
        {  4, testFASTSplitPredictor_belowThreshold,       "FASTSplitPredictor_belowThreshold" },
        {  5, testFASTSplitPredictor_predictWithoutInit,   "FASTSplitPredictor_predictWithoutInit" },
        {  6, testFASTSplitPredictor_doubleInit,           "FASTSplitPredictor_doubleInit" },
        {  7, testFASTSplitPredictor_releaseCycle,         "FASTSplitPredictor_releaseCycle" },
        {  8, testFASTSplitPredictor_noModelsDir,          "FASTSplitPredictor_noModelsDir" },
        {  9, testCUFeatureExtractor_defaultFeatures,      "CUFeatureExtractor_defaultFeatures" },
    };

    int numTests = sizeof(tests) / sizeof(tests[0]);

    for (int i = 0; i < numTests; ++i)
    {
        if (runAll || tests[i].id == testId)
        {
            int res = tests[i].func();
            std::cout << (res == 0 ? "PASS" : "FAIL") << ": " << tests[i].name << std::endl;
            g_numTests++;
            g_numFails += res;
        }
    }

    // Cleanup temp directory
    rmdir(TEST_DIR);

    if (g_numFails > 0)
    {
        std::cerr << "\n" << g_numFails << " of " << g_numTests << " tests FAILED." << std::endl;
        return 1;
    }

    std::cout << "\nAll " << g_numTests << " tests passed." << std::endl;
    return 0;
}
