/** \file     CUFeatureExtractor.cpp
    \brief    Mansouri 2024 CU feature extraction (implementation)
*/

#include "CUFeatureExtractor.h"

#include "CommonLib/Unit.h"
#include "CommonLib/CodingStructure.h"
#include "CommonLib/UnitPartitioner.h"
#include "CommonLib/UnitTools.h"
#include "CommonLib/Mv.h"
#include "CommonLib/TrQuant.h"

#include <cmath>
#include <algorithm>

namespace vvenc {

CUFeatureExtractor::CUFeatureExtractor()
{
    m_vFeatures.reserve(NUM_FEATURES);
}

CUFeatureExtractor::~CUFeatureExtractor() = default;

void CUFeatureExtractor::xReset()
{
    m_vFeatures.clear();
}

int CUFeatureExtractor::extract(const CodingUnit& cu,
                                const Partitioner& partitioner,
                                std::vector<double>& outFeatures)
{
    xReset();

    if (!cu.cs) return -1;

    if (xAddTextureFeatures(cu) != 0) return -1;
    if (xAddNeighborFeatures(cu) != 0) return -1;
    if (xAddContextFeatures(cu, partitioner) != 0) return -1;
    if (xAddMotionFeatures(cu) != 0) return -1;
    if (xAddResidualFeatures(cu) != 0) return -1;

    outFeatures = m_vFeatures;
    return 0;
}

int CUFeatureExtractor::xAddTextureFeatures(const CodingUnit& cu)
{
    const CompArea& lumaArea = cu.Y();
    const int width  = lumaArea.width;
    const int height = lumaArea.height;

    if (width <= 0 || height <= 0)
    {
        m_vFeatures.insert(m_vFeatures.end(), 6, 0.0);
        return 0;
    }

    const CPelBuf lumaBuf = cu.cs->getOrgBuf(lumaArea);

    double sum    = 0.0;
    double sumSq  = 0.0;
    double gradH  = 0.0;
    double gradV  = 0.0;

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const double val = static_cast<double>(lumaBuf.at(x, y));
            sum   += val;
            sumSq += val * val;

            if (x > 0 && x < width - 1)
            {
                gradH += std::abs(static_cast<double>(
                    lumaBuf.at(x + 1, y) - lumaBuf.at(x - 1, y)));
            }
            if (y > 0 && y < height - 1)
            {
                gradV += std::abs(static_cast<double>(
                    lumaBuf.at(x, y + 1) - lumaBuf.at(x, y - 1)));
            }
        }
    }

    const double n   = static_cast<double>(width * height);
    const double mean   = sum / n;
    const double var    = (sumSq / n) - (mean * mean);
    const double gradMag = (gradH + gradV) / (n * 2.0);
    const double edgeStr = (gradV > 1.0) ? (gradH / gradV) : 1.0;
    const double acEng   = var;
    const double totalEn = var + (mean * mean);
    const double lfRatio = (totalEn > 1e-10) ? (mean * mean) / totalEn : 0.0;

    m_vFeatures.push_back(std::min(var / 10000.0, 1.0));
    m_vFeatures.push_back(std::min(gradMag / 500.0, 1.0));
    m_vFeatures.push_back(std::min(edgeStr / 10.0, 1.0));
    m_vFeatures.push_back(std::min(mean / 1023.0, 1.0));
    m_vFeatures.push_back(std::min(acEng / 10000.0, 1.0));
    m_vFeatures.push_back(lfRatio);

    return 0;
}

int CUFeatureExtractor::xAddNeighborFeatures(const CodingUnit& cu)
{
    const CodingUnit* leftCu = CU::getLeft(cu);
    const CodingUnit* aboveCu = CU::getAbove(cu);

    auto getDepth = [](const CodingUnit* n) -> double {
        return n ? std::min(static_cast<double>(n->depth) / 6.0, 1.0) : 0.0;
    };
    auto getMode = [](const CodingUnit* n) -> double {
        if (!n) return 0.0;
        return (n->predMode == MODE_INTER) ? 1.0 : 0.5;
    };

    m_vFeatures.push_back(getDepth(leftCu));
    m_vFeatures.push_back(getDepth(aboveCu));
    m_vFeatures.push_back(getDepth(cu.cs->getCU(cu.lumaPos().offset(-1, -1), cu.chType, cu.treeType)));

    m_vFeatures.push_back(getMode(leftCu));
    m_vFeatures.push_back(getMode(aboveCu));

    const CodingUnit* tl = cu.cs->getCU(cu.lumaPos().offset(-1, -1), cu.chType, cu.treeType);
    m_vFeatures.push_back(getMode(tl));

    return 0;
}

int CUFeatureExtractor::xAddContextFeatures(const CodingUnit& cu,
                                            const Partitioner& partitioner)
{
    const double log2Size = std::log2(static_cast<double>(cu.lumaSize().width));
    const double sizeNorm = (log2Size - 2.0) / 4.0;
    const double depthNorm = std::min(static_cast<double>(cu.depth) / 6.0, 1.0);
    const double qpNorm   = std::min(static_cast<double>(cu.qp) / 63.0, 1.0);

    m_vFeatures.push_back(std::max(0.0, std::min(sizeNorm, 1.0)));
    m_vFeatures.push_back(depthNorm);
    m_vFeatures.push_back(qpNorm);

    return 0;
}

int CUFeatureExtractor::xAddMotionFeatures(const CodingUnit& cu)
{
    double mvMag      = 0.0;
    double mvDiffLeft = 0.0;
    double mvDiffAbove = 0.0;
    double mergeCost  = 0.0;
    double refIdx     = 0.0;

    if (cu.predMode == MODE_INTER)
    {
        const Mv& mv0 = cu.mv[REF_PIC_LIST_0][0];
        mvMag = std::sqrt(static_cast<double>(mv0.hor) * mv0.hor
                        + static_cast<double>(mv0.ver) * mv0.ver)
              / 16.0;

        const CodingUnit* leftCu  = CU::getLeft(cu);
        const CodingUnit* aboveCu = CU::getAbove(cu);

        if (leftCu && leftCu->predMode == MODE_INTER)
        {
            const Mv& leftMv = leftCu->mv[REF_PIC_LIST_0][0];
            mvDiffLeft = std::abs(static_cast<double>(mv0.hor - leftMv.hor))
                       + std::abs(static_cast<double>(mv0.ver - leftMv.ver));
        }

        if (aboveCu && aboveCu->predMode == MODE_INTER)
        {
            const Mv& aboveMv = aboveCu->mv[REF_PIC_LIST_0][0];
            mvDiffAbove = std::abs(static_cast<double>(mv0.hor - aboveMv.hor))
                        + std::abs(static_cast<double>(mv0.ver - aboveMv.ver));
        }

        refIdx = (cu.refIdx[REF_PIC_LIST_0] >= 0) ? 1.0 : 0.0;
    }

    m_vFeatures.push_back(std::min(mvMag / 1000.0, 1.0));
    m_vFeatures.push_back(std::min(mvDiffLeft / 1000.0, 1.0));
    m_vFeatures.push_back(std::min(mvDiffAbove / 1000.0, 1.0));
    m_vFeatures.push_back(std::min(mergeCost / 100000.0, 1.0));
    m_vFeatures.push_back(refIdx);

    return 0;
}

int CUFeatureExtractor::xAddResidualFeatures(const CodingUnit& cu)
{
    int    coeffCount = 0;
    double sad        = 0.0;

    if (cu.rootCbf)
    {
        for (const auto& tu : CU::traverseTUs(const_cast<CodingUnit&>(cu)))
        {
            if (tu.cbf[COMP_Y] > 0)
            {
                const CCoeffSigBuf coeffs = tu.getCoeffs(COMP_Y);
                const int numCoeffs = coeffs.width * coeffs.height;
                for (int i = 0; i < numCoeffs; ++i)
                {
                    if (coeffs.buf[i] != 0)
                    {
                        ++coeffCount;
                        sad += std::abs(static_cast<double>(coeffs.buf[i]));
                    }
                }
            }
        }
    }

    m_vFeatures.push_back(std::min(sad / 100000.0, 1.0));
    m_vFeatures.push_back(std::min(static_cast<double>(coeffCount) / 256.0, 1.0));

    return 0;
}

}
