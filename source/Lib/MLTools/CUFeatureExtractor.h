/** \file     CUFeatureExtractor.h
    \brief    Mansouri 2024 CU feature extraction (header)
*/

#pragma once

#include <vector>

namespace vvenc {

class CodingUnit;
class Partitioner;

class CUFeatureExtractor {
public:
    static constexpr int NUM_FEATURES = 22;

    CUFeatureExtractor();
    virtual ~CUFeatureExtractor();

    int extract(const CodingUnit& cu,
                const Partitioner& partitioner,
                std::vector<double>& outFeatures);

private:
    int xAddTextureFeatures(const CodingUnit& cu);
    int xAddNeighborFeatures(const CodingUnit& cu);
    int xAddContextFeatures(const CodingUnit& cu, const Partitioner& partitioner);
    int xAddMotionFeatures(const CodingUnit& cu);
    int xAddResidualFeatures(const CodingUnit& cu);
    void xReset();

    std::vector<double> m_vFeatures;
};

}
