/** \file     CUFeatureExtractor.cpp
    \brief    Taabane 2024 CU feature extraction (implementation)
             Features: texture, SCTC, neighbour, context (QTD/MTTD/Tid), MV variance, residual
*/

#include "CUFeatureExtractor.h"

#include "CommonLib/Unit.h"
#include "CommonLib/CodingStructure.h"
#include "CommonLib/UnitPartitioner.h"
#include "CommonLib/UnitTools.h"
#include "CommonLib/Mv.h"
#include "CommonLib/TrQuant.h"
#include "CommonLib/Slice.h"

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

    if (!cu.cs || !cu.cs->sps) return -1;

    if (xAddTextureFeatures(cu) != 0) return -1;
    if (xAddSCTCFeatures(cu) != 0) return -1;
    if (xAddNeighborFeatures(cu) != 0) return -1;
    if (xAddContextFeatures(cu, partitioner) != 0) return -1;
    if (xAddMotionFeatures(cu) != 0) return -1;
    if (xAddResidualFeatures(cu) != 0) return -1;

    outFeatures = m_vFeatures;
    return 0;
}

// ---------------------------------------------------------------------------
// Texture features: indices 0-6
// ---------------------------------------------------------------------------
int CUFeatureExtractor::xAddTextureFeatures(const CodingUnit& cu)
{
    const CompArea& lumaArea = cu.Y();
    const int width  = lumaArea.width;
    const int height = lumaArea.height;

    if (width <= 0 || height <= 0)
    {
        m_vFeatures.insert(m_vFeatures.end(), 7, 0.0);
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

    const double n      = static_cast<double>(width * height);
    const double mean   = sum / n;
    const double var    = (sumSq / n) - (mean * mean);
    const double edgeStr = (gradV > 1.0) ? (gradH / gradV) : 1.0;
    const double acEng   = var;
    const double totalEn = var + (mean * mean);
    const double lfRatio = (totalEn > 1e-10) ? (mean * mean) / totalEn : 0.0;

    m_vFeatures.push_back(std::min(var / 10000.0, 1.0));                     // idx 0: variance
    m_vFeatures.push_back(std::min(gradH / (n * 2.0) / 500.0, 1.0));        // idx 1: horizontal gradient
    m_vFeatures.push_back(std::min(gradV / (n * 2.0) / 500.0, 1.0));        // idx 2: vertical gradient
    m_vFeatures.push_back(std::min(edgeStr / 10.0, 1.0));                   // idx 3: edge strength
    m_vFeatures.push_back(std::min(mean / 1023.0, 1.0));                    // idx 4: DC / mean
    m_vFeatures.push_back(std::min(acEng / 10000.0, 1.0));                  // idx 5: AC energy
    m_vFeatures.push_back(lfRatio);                                          // idx 6: LF ratio

    return 0;
}

// ---------------------------------------------------------------------------
// Sub-CU Texture Complexity: indices 7-11 (Taabane 2024 eq.6)
// ---------------------------------------------------------------------------
int CUFeatureExtractor::xAddSCTCFeatures(const CodingUnit& cu)
{
    const CompArea& lumaArea = cu.Y();
    const int cuW = lumaArea.width;
    const int cuH = lumaArea.height;
    if (cuW <= 0 || cuH <= 0)
    {
        m_vFeatures.insert(m_vFeatures.end(), 5, 0.0);
        return 0;
    }
    const CPelBuf lumaBuf = cu.cs->getOrgBuf(lumaArea);

    auto subCuVariance = [&](int sx, int sy, int sw, int sh) -> double {
        if (sw <= 0 || sh <= 0) return 0.0;
        double s = 0.0, sq = 0.0;
        const double n = static_cast<double>(sw * sh);
        for (int y = 0; y < sh; ++y)
            for (int x = 0; x < sw; ++x)
            {
                double v = static_cast<double>(lumaBuf.at(x + sx, y + sy));
                s  += v;
                sq += v * v;
            }
        return (sq / n) - (s / n) * (s / n);
    };

    auto sctc = [&](int parts, int pw, int ph, int pcount) -> double {
        // parts: number of sub-CUs; pw/ph: base sub-CU dimensions; pcount: how many per dimension
        double sumVar = 0.0;
        double vars[6];
        int idx = 0;
        for (int i = 0; i < pcount && idx < parts; ++i)
            for (int j = 0; j < pcount && idx < parts; ++j)
            {
                vars[idx] = subCuVariance(j * pw, i * ph, pw, ph);
                sumVar += vars[idx];
                ++idx;
            }
        double meanVar = sumVar / static_cast<double>(parts);
        double acc = 0.0;
        for (int i = 0; i < parts; ++i)
            acc += (vars[i] - meanVar) * (vars[i] - meanVar);
        return acc / static_cast<double>(parts);
    };

    // Each split mode has a known number of sub-CUs with dimensions
    // QT: 4 sub-CUs (2x2 grid)
    // BH: 2 sub-CUs (1x2 vertical stack)
    // BV: 2 sub-CUs (2x1 horizontal pair)
    // TH: 3 sub-CUs (1x3 vertical stack)
    // TV: 3 sub-CUs (3x1 horizontal row)
    // sub-CU dimensions are half the CU size in the split direction

    int hw = cuW / 2, hh = cuH / 2;
    double sctcQt = sctc(4, hw, hh, 2);

    double sctcBh = sctc(2, cuW, hh, 2);
    double sctcBv = sctc(2, hw, cuH, 2);

    int tw = cuW / 3, th = cuH / 3;
    double sctcTh = sctc(3, cuW, th, 3);
    double sctcTv = sctc(3, tw, cuH, 3);

    // Normalise by max plausible SCTC (10^8 for 10-bit video)
    const double SCTC_SCALE = 100000000.0;
    m_vFeatures.push_back(std::min(sctcQt / SCTC_SCALE, 1.0));   // idx 7: SCTC for QT
    m_vFeatures.push_back(std::min(sctcBh / SCTC_SCALE, 1.0));   // idx 8: SCTC for BH
    m_vFeatures.push_back(std::min(sctcBv / SCTC_SCALE, 1.0));   // idx 9: SCTC for BV
    m_vFeatures.push_back(std::min(sctcTh / SCTC_SCALE, 1.0));   // idx 10: SCTC for TH
    m_vFeatures.push_back(std::min(sctcTv / SCTC_SCALE, 1.0));   // idx 11: SCTC for TV

    return 0;
}

// ---------------------------------------------------------------------------
// Neighbour features: indices 12-17
// ---------------------------------------------------------------------------
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

    m_vFeatures.push_back(getDepth(leftCu));      // idx 12
    m_vFeatures.push_back(getDepth(aboveCu));      // idx 13
    m_vFeatures.push_back(getDepth(cu.cs->getCU(cu.lumaPos().offset(-1, -1), cu.chType, cu.treeType))); // idx 14

    m_vFeatures.push_back(getMode(leftCu));        // idx 15
    m_vFeatures.push_back(getMode(aboveCu));       // idx 16

    const CodingUnit* tl = cu.cs->getCU(cu.lumaPos().offset(-1, -1), cu.chType, cu.treeType);
    m_vFeatures.push_back(getMode(tl));            // idx 17

    return 0;
}

// ---------------------------------------------------------------------------
// Context features: indices 18-22
// ---------------------------------------------------------------------------
int CUFeatureExtractor::xAddContextFeatures(const CodingUnit& cu,
                                            const Partitioner& partitioner)
{
    const double log2Size = std::log2(static_cast<double>(cu.lumaSize().width));
    const double sizeNorm = (log2Size - 2.0) / 4.0;
    const double qpNorm   = std::min(static_cast<double>(cu.qp) / 63.0, 1.0);

    // QT depth = cu.depth (total splits applied)
    // MTT depth = cu.btDepth or cu.mtDepth (BT/TT splits after QT)
    const double qtDepthNorm  = std::min(static_cast<double>(cu.depth) / 6.0, 1.0);
    const double mttDepthNorm = std::min(static_cast<double>(cu.mtDepth) / 4.0, 1.0);

    // Temporal layer
    const double tidNorm = (cu.slice && cu.slice->sps)
        ? std::min(static_cast<double>(cu.slice->TLayer) / 7.0, 1.0)
        : 0.0;

    m_vFeatures.push_back(std::max(0.0, std::min(sizeNorm, 1.0)));     // idx 18: log2 size
    m_vFeatures.push_back(qtDepthNorm);                                  // idx 19: QT depth (QTD)
    m_vFeatures.push_back(mttDepthNorm);                                 // idx 20: MTT depth (MTTD)
    m_vFeatures.push_back(qpNorm);                                       // idx 21: QP
    m_vFeatures.push_back(tidNorm);                                      // idx 22: Temporal layer (Tid)

    return 0;
}

// ---------------------------------------------------------------------------
// Motion features: indices 23-28
// ---------------------------------------------------------------------------
int CUFeatureExtractor::xAddMotionFeatures(const CodingUnit& cu)
{
    double mvVarH  = 0.0;
    double mvVarV  = 0.0;
    double mvDiffLeft  = 0.0;
    double mvDiffAbove = 0.0;
    double mergeCost   = 0.0;
    double refIdx      = 0.0;

    if (cu.predMode == MODE_INTER)
    {
        const int cuW = cu.lumaSize().width;
        const int cuH = cu.lumaSize().height;
        const int numBlocks = (cuW / 4) * (cuH / 4);

        if (numBlocks > 0)
        {
            double sumH = 0.0, sumV = 0.0;
            double sumHSq = 0.0, sumVSq = 0.0;

            // Iterate over 4x4 blocks within the CU to collect MV components
            for (int by = 0; by < cuH; by += 4)
            {
                for (int bx = 0; bx < cuW; bx += 4)
                {
                    // Get MV for this 4x4 block position
                    const Position pos(cu.lumaPos().x + bx, cu.lumaPos().y + by);
                    const CodingUnit* blkCu = cu.cs->getCU(pos, cu.chType, cu.treeType);
                    int mvHor = 0, mvVer = 0;
                    if (blkCu && blkCu->predMode == MODE_INTER)
                    {
                        mvHor = blkCu->mv[REF_PIC_LIST_0][0].hor;
                        mvVer = blkCu->mv[REF_PIC_LIST_0][0].ver;
                    }
                    const double h = static_cast<double>(mvHor) / 16.0;
                    const double v = static_cast<double>(mvVer) / 16.0;
                    sumH  += h;  sumV  += v;
                    sumHSq += h * h;  sumVSq += v * v;
                }
            }

            const double n = static_cast<double>(numBlocks);
            const double meanH = sumH / n;
            const double meanV = sumV / n;
            mvVarH = (sumHSq / n) - (meanH * meanH);
            mvVarV = (sumVSq / n) - (meanV * meanV);
        }

        const Mv& mv0 = cu.mv[REF_PIC_LIST_0][0];

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

    m_vFeatures.push_back(std::min(mvVarH / 1000000.0, 1.0));    // idx 23: MV variance H
    m_vFeatures.push_back(std::min(mvVarV / 1000000.0, 1.0));    // idx 24: MV variance V
    m_vFeatures.push_back(std::min(mvDiffLeft / 1000.0, 1.0));   // idx 25: MV diff left
    m_vFeatures.push_back(std::min(mvDiffAbove / 1000.0, 1.0));  // idx 26: MV diff above
    m_vFeatures.push_back(std::min(mergeCost / 100000.0, 1.0));  // idx 27: merge cost
    m_vFeatures.push_back(refIdx);                                 // idx 28: reference index

    return 0;
}

// ---------------------------------------------------------------------------
// Residual features: indices 29-30
// ---------------------------------------------------------------------------
int CUFeatureExtractor::xAddResidualFeatures(const CodingUnit& cu)
{
    int    coeffCount = 0;
    double sad        = 0.0;

    if (cu.rootCbf && cu.firstTU)
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

    m_vFeatures.push_back(std::min(sad / 100000.0, 1.0));                    // idx 29: SAD
    m_vFeatures.push_back(std::min(static_cast<double>(coeffCount) / 256.0, 1.0)); // idx 30: coeff count

    return 0;
}

double CUFeatureExtractor::xComputeSubBlockVariance(
    const CodingUnit& cu,
    double(CodingUnit::*getVal)(int, int) const)
{
    (void)cu;
    (void)getVal;
    return 0.0;
}

}
