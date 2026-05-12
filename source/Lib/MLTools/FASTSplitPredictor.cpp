/** \file     FASTSplitPredictor.cpp
    \brief    LightGBM-based CU split predictor (implementation)
*/

#include "FASTSplitPredictor.h"

#include <algorithm>
#include <cstring>
#include <iostream>

#if VVENC_ENABLE_ML_LIGHTGBM
#include <LightGBM/c_api.h>
#endif

namespace vvenc {

FASTSplitPredictor* FASTSplitPredictor::s_pInstance = nullptr;

static const char* MODEL_NAMES[FASTSplitPredictor::NUM_MODELS] = {
    "qt_split_model.txt",
    "bh_split_model.txt",
    "bv_split_model.txt",
    "th_split_model.txt",
    "tv_split_model.txt"
};

FASTSplitPredictor::FASTSplitPredictor()
    : m_bInitialized(false)
{
    for (int i = 0; i < NUM_MODELS; ++i)
        m_hBoosters[i] = nullptr;
}

FASTSplitPredictor::~FASTSplitPredictor()
{
    release();
}

int FASTSplitPredictor::init(const std::string& modelDir)
{
#if VVENC_ENABLE_ML_LIGHTGBM
    if (m_bInitialized)
        return -1;

    m_cModelDir = modelDir;

    for (int i = 0; i < NUM_MODELS; ++i)
    {
        std::string path = modelDir + "/" + MODEL_NAMES[i];
        int ret = xLoadBooster(path, reinterpret_cast<void*>(&m_hBoosters[i]));
        if (ret != 0)
        {
            std::cerr << "[ML] Failed to load model: " << path << std::endl;
            release();
            return -1;
        }
    }

    m_bInitialized = true;
    std::cerr << "[ML] FASTSplitPredictor loaded " << NUM_MODELS
              << " models from " << modelDir << std::endl;
    return 0;
#else
    (void)modelDir;
    return -1;
#endif
}

int FASTSplitPredictor::predict(const std::vector<double>& features,
                                double confidenceThreshold,
                                SplitType& outSplit,
                                double& outConfidence)
{
#if VVENC_ENABLE_ML_LIGHTGBM
    if (!m_bInitialized)
    {
        outSplit = NO_SPLIT;
        outConfidence = 0.0;
        return -1;
    }

    double scores[NUM_MODELS];
    for (int i = 0; i < NUM_MODELS; ++i)
    {
        double result = 0.0;
        int ret = xPredictOne(m_hBoosters[i], features, result);
        if (ret != 0)
            result = 0.0;
        scores[i] = result;
    }

    int bestIdx = 0;
    double maxScore = scores[0];
    for (int i = 1; i < NUM_MODELS; ++i)
    {
        if (scores[i] > maxScore)
        {
            maxScore = scores[i];
            bestIdx = i;
        }
    }

    outConfidence = maxScore;

    if (maxScore >= confidenceThreshold)
    {
        static const SplitType splitMap[NUM_MODELS] = {
            QT_SPLIT, BH_SPLIT, BV_SPLIT, TH_SPLIT, TV_SPLIT
        };
        outSplit = splitMap[bestIdx];
    }
    else
    {
        outSplit = NO_SPLIT;
    }

    return 0;
#else
    (void)features;
    (void)confidenceThreshold;
    outSplit = NO_SPLIT;
    outConfidence = 0.0;
    return 0;
#endif
}

int FASTSplitPredictor::release()
{
#if VVENC_ENABLE_ML_LIGHTGBM
    for (int i = 0; i < NUM_MODELS; ++i)
    {
        if (m_hBoosters[i])
        {
            LGBM_BoosterFree(reinterpret_cast<BoosterHandle>(m_hBoosters[i]));
            m_hBoosters[i] = nullptr;
        }
    }
    m_bInitialized = false;
#endif
    return 0;
}

bool FASTSplitPredictor::isInitialized() const
{
    return m_bInitialized;
}

FASTSplitPredictor* FASTSplitPredictor::getInstance()
{
    return s_pInstance;
}

void FASTSplitPredictor::setInstance(FASTSplitPredictor* predictor)
{
    s_pInstance = predictor;
}

int FASTSplitPredictor::xLoadBooster(const std::string& path, void* handle)
{
#if VVENC_ENABLE_ML_LIGHTGBM
    BoosterHandle* outHandle = reinterpret_cast<BoosterHandle*>(handle);
    int outIter = 0;
    int ret = LGBM_BoosterCreateFromModelfile(path.c_str(), &outIter, outHandle);
    return (ret == 0) ? 0 : -1;
#else
    (void)path;
    (void)handle;
    return -1;
#endif
}

int FASTSplitPredictor::xPredictOne(void* handle,
                                    const std::vector<double>& features,
                                    double& result)
{
#if VVENC_ENABLE_ML_LIGHTGBM
    if (!handle)
        return -1;

    BoosterHandle booster = reinterpret_cast<BoosterHandle>(handle);
    int64_t outLen = 0;
    double outResult = 0.0;

    int ret = LGBM_BoosterPredictForMat(
        booster,
        const_cast<void*>(static_cast<const void*>(features.data())),
        C_API_DTYPE_FLOAT64,
        1,
        static_cast<int>(features.size()),
        1,
        C_API_PREDICT_NORMAL,
        0,
        -1,
        "",
        &outLen,
        &outResult);

    if (ret == 0 && outLen > 0)
        result = outResult;
    else
        result = 0.0;

    return (ret == 0) ? 0 : -1;
#else
    (void)handle;
    (void)features;
    result = 0.0;
    return -1;
#endif
}

}
