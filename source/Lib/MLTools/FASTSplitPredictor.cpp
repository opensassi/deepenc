/** \file     FASTSplitPredictor.cpp
    \brief    LightGBM-based CU split predictor — Top-N algorithm (Taabane 2024)
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

static const FASTSplitPredictor::SplitType MODEL_SPLITS[FASTSplitPredictor::NUM_MODELS] = {
    FASTSplitPredictor::QT_SPLIT,
    FASTSplitPredictor::BH_SPLIT,
    FASTSplitPredictor::BV_SPLIT,
    FASTSplitPredictor::TH_SPLIT,
    FASTSplitPredictor::TV_SPLIT
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
                                int nCandidates,
                                double thNs,
                                unsigned int allowedSplits,
                                std::vector<std::pair<SplitType, double>>& outCandidates,
                                bool& bEarlySkip)
{
#if VVENC_ENABLE_ML_LIGHTGBM
    if (!m_bInitialized)
    {
        bEarlySkip = true;
        return -1;
    }

    outCandidates.clear();
    bEarlySkip = false;

    // Collect scores for allowed split types only
    std::vector<std::pair<SplitType, double>> allScores;
    for (int i = 0; i < NUM_MODELS; ++i)
    {
        SplitType st = MODEL_SPLITS[i];
        if (!(allowedSplits & (1u << static_cast<unsigned int>(st))))
            continue;

        double result = 0.0;
        int ret = xPredictOne(m_hBoosters[i], features, result);
        if (ret == 0)
            allScores.emplace_back(st, result);
    }

    if (allScores.empty())
    {
        bEarlySkip = true;
        return 0;
    }

    // Sort descending by confidence
    std::sort(allScores.begin(), allScores.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });

    // Check against no-skip threshold
    if (allScores[0].second < thNs)
    {
        bEarlySkip = true;
        return 0;
    }

    // Take top-N
    int count = std::min(nCandidates, static_cast<int>(allScores.size()));
    for (int i = 0; i < count; ++i)
        outCandidates.push_back(allScores[i]);

    return 0;
#else
    (void)features;
    (void)nCandidates;
    (void)thNs;
    (void)allowedSplits;
    outCandidates.clear();
    bEarlySkip = true;
    return -1;
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

int FASTSplitPredictor::splitTypeToPartSplit(SplitType type)
{
    switch (type)
    {
    case QT_SPLIT: return 0;  // CU_QUAD_SPLIT = 0
    case BH_SPLIT: return 2;  // CU_HORZ_SPLIT = 2
    case BV_SPLIT: return 1;  // CU_VERT_SPLIT = 1
    case TH_SPLIT: return 4;  // CU_TRIH_SPLIT = 4
    case TV_SPLIT: return 3;  // CU_TRIV_SPLIT = 3
    default:       return 2000; // CU_DONT_SPLIT = 2000
    }
}

int FASTSplitPredictor::xSplitToIndex(SplitType type)
{
    switch (type)
    {
    case QT_SPLIT: return 0;
    case BH_SPLIT: return 1;
    case BV_SPLIT: return 2;
    case TH_SPLIT: return 3;
    case TV_SPLIT: return 4;
    default:       return -1;
    }
}

FASTSplitPredictor::SplitType FASTSplitPredictor::xIndexToSplit(int idx)
{
    if (idx >= 0 && idx < NUM_MODELS)
        return MODEL_SPLITS[idx];
    return NO_SPLIT;
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
