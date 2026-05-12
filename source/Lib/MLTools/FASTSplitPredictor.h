/** \file     FASTSplitPredictor.h
    \brief    LightGBM-based CU split predictor (header)
*/

#pragma once

#include <string>
#include <vector>

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
    int predict(const std::vector<double>& features,
                double confidenceThreshold,
                SplitType& outSplit,
                double& outConfidence);
    int release();
    bool isInitialized() const;

    static constexpr int NUM_MODELS = 5;

    static FASTSplitPredictor* getInstance();
    static void setInstance(FASTSplitPredictor* predictor);

private:
    int xLoadBooster(const std::string& path, void* handle);
    int xPredictOne(void* handle,
                    const std::vector<double>& features,
                    double& result);

    void*       m_hBoosters[NUM_MODELS];
    bool        m_bInitialized;
    std::string m_cModelDir;

    static FASTSplitPredictor* s_pInstance;
};

}
