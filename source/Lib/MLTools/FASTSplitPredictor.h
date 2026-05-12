/** \file     FASTSplitPredictor.h
    \brief    LightGBM-based CU split predictor — Top-N algorithm (Taabane 2024)
*/

#pragma once

#include <string>
#include <vector>
#include <utility>

namespace vvenc {

class FASTSplitPredictor {
public:
    enum SplitType {
        NO_SPLIT = 0,
        QT_SPLIT,
        BH_SPLIT,
        BV_SPLIT,
        TH_SPLIT,
        TV_SPLIT
    };

    FASTSplitPredictor();
    virtual ~FASTSplitPredictor();

    int init(const std::string& modelDir);

    /** \brief Predict top-N candidate splits (Taabane 2024 Algorithm 1)
     *  \param[in]  features       ~31-element feature vector
     *  \param[in]  nCandidates    Max candidates to return (top-N)
     *  \param[in]  thNs           No-skip threshold. If max(pred) < thNs, bEarlySkip=true
     *  \param[in]  allowedSplits  Bitmask of SplitType values valid for current CU geometry
     *  \param[out] outCandidates  Descending-ranked list of (SplitType, confidence)
     *  \param[out] bEarlySkip     true if best score < thNs — CU should encode without splitting
     *  \retval 0  Success
     *  \retval -1 Not initialised
     */
    int predict(const std::vector<double>& features,
                int nCandidates,
                double thNs,
                unsigned int allowedSplits,
                std::vector<std::pair<SplitType, double>>& outCandidates,
                bool& bEarlySkip);

    int release();
    bool isInitialized() const;

    /** \brief Map SplitType to PartSplit integer code */
    static int splitTypeToPartSplit(SplitType type);

    static constexpr int NUM_MODELS = 5;

    static FASTSplitPredictor* getInstance();
    static void setInstance(FASTSplitPredictor* predictor);

private:
    int xLoadBooster(const std::string& path, void* handle);
    int xPredictOne(void* handle,
                    const std::vector<double>& features,
                    double& result);

    static int xSplitToIndex(SplitType type);
    static SplitType xIndexToSplit(int idx);

    void*       m_hBoosters[NUM_MODELS];
    bool        m_bInitialized;
    std::string m_cModelDir;

    static FASTSplitPredictor* s_pInstance;
};

}
