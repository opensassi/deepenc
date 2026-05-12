/** \file     CUFeatureExtractor.h
    \brief    Taabane 2024 CU feature extraction (header)
*/

#pragma once

#include <vector>

namespace vvenc {

class CodingUnit;
class Partitioner;

class CUFeatureExtractor {
public:
    static constexpr int NUM_FEATURES = 31;

    CUFeatureExtractor();
    virtual ~CUFeatureExtractor();

    int extract(const CodingUnit& cu,
                const Partitioner& partitioner,
                std::vector<double>& outFeatures);

private:
    int xAddTextureFeatures(const CodingUnit& cu);
    int xAddSCTCFeatures(const CodingUnit& cu);
    int xAddNeighborFeatures(const CodingUnit& cu);
    int xAddContextFeatures(const CodingUnit& cu, const Partitioner& partitioner);
    int xAddMotionFeatures(const CodingUnit& cu);
    int xAddResidualFeatures(const CodingUnit& cu);

    double xComputeSubBlockVariance(const CodingUnit& cu,
                                    double(CodingUnit::*getVal)(int, int) const);

    void xReset();

    std::vector<double> m_vFeatures;
};

}
