#include "HWCuPartitionAnalyzer.h"

#include <cmath>
#include <cstring>
#include <vector>

namespace vvenc {

HWCuPartitionAnalyzer::HWCuPartitionAnalyzer()
{
}

HWCuPartitionAnalyzer::~HWCuPartitionAnalyzer()
{
}

int HWCuPartitionAnalyzer::computeHint(const MBPartitionGrid& rcGrid,
                                       int iCtuX, int iCtuY, int iCUSize,
                                       CUSplitHint& rcHint) const
{
  rcHint.m_bForceSplit = false;
  rcHint.m_bNoSplit    = false;
  rcHint.m_eSplitType  = CU_SPLIT_NONE;
  rcHint.m_fConfidence = 0.0f;

  int originX = iCtuX * iCUSize;
  int originY = iCtuY * iCUSize;

  int maxSize = iCUSize;
  std::vector<uint8_t> mbTypes(maxSize * maxSize);
  std::vector<HWMV>    mbMvs(maxSize * maxSize);
  int actualW = 0, actualH = 0;

  int ret = extractSubGrid(rcGrid, originX, originY, iCUSize,
                           mbTypes.data(), mbMvs.data(),
                           actualW, actualH);
  if (ret != 0)
    return 1;

  int count = actualW * actualH;
  if (count == 0)
    return 1;

  float mvVar    = computeMVVariance(mbMvs.data(), count);
  float entropy  = computePartitionEntropy(mbTypes.data(), count);
  bool  hBound   = false;
  bool  vBound   = false;
  bool  hasBound = hasMotionBoundary(mbMvs.data(), actualW, actualH,
                                     hBound, vBound);

  float coverage = (float)count / (float)(iCUSize * iCUSize);

  if (hasBound)
  {
    rcHint.m_bNoSplit    = false;
    rcHint.m_bForceSplit = true;
    rcHint.m_eSplitType  = determineSplitType(hBound, vBound);
    rcHint.m_fConfidence = 0.92f * coverage;
  }
  else if (mvVar < LOW_MV_VAR_THRESHOLD && entropy < LOW_ENTROPY_THRESHOLD)
  {
    rcHint.m_bNoSplit    = true;
    rcHint.m_bForceSplit = false;
    rcHint.m_eSplitType  = CU_SPLIT_NONE;
    rcHint.m_fConfidence = 0.9f * coverage;
  }
  else if (mvVar < MED_MV_VAR_THRESHOLD && entropy < MED_ENTROPY_THRESHOLD)
  {
    rcHint.m_bNoSplit    = true;
    rcHint.m_bForceSplit = false;
    rcHint.m_eSplitType  = CU_SPLIT_NONE;
    rcHint.m_fConfidence = 0.6f * coverage;
  }
  else if (mvVar > HIGH_MV_VAR_THRESHOLD || entropy > HIGH_ENTROPY_THRESHOLD)
  {
    rcHint.m_bNoSplit    = false;
    rcHint.m_bForceSplit = true;
    rcHint.m_eSplitType  = CU_SPLIT_QT;
    rcHint.m_fConfidence = 0.85f * coverage;
  }
  else
  {
    rcHint.m_bNoSplit    = false;
    rcHint.m_bForceSplit = false;
    rcHint.m_eSplitType  = CU_SPLIT_NONE;
    rcHint.m_fConfidence = 0.3f * coverage;
  }

  return 0;
}

float HWCuPartitionAnalyzer::computeMVVariance(const HWMV* pcMVs,
                                               int iCount) const
{
  if (iCount <= 1)
    return 0.0f;

  float meanX = 0.0f, meanY = 0.0f;
  for (int i = 0; i < iCount; i++)
  {
    meanX += (float)pcMVs[i].x;
    meanY += (float)pcMVs[i].y;
  }
  meanX /= (float)iCount;
  meanY /= (float)iCount;

  float variance = 0.0f;
  for (int i = 0; i < iCount; i++)
  {
    float dx = (float)pcMVs[i].x - meanX;
    float dy = (float)pcMVs[i].y - meanY;
    variance += dx * dx + dy * dy;
  }
  variance /= (float)iCount;

  float maxDistSq = 2.0f * 32767.0f * 32767.0f;
  float normalized = variance / maxDistSq;
  if (normalized > 1.0f)
    normalized = 1.0f;

  return normalized;
}

float HWCuPartitionAnalyzer::computePartitionEntropy(const uint8_t* pcTypes,
                                                     int iCount) const
{
  if (iCount <= 1)
    return 0.0f;

  int bins[256];
  memset(bins, 0, sizeof(bins));
  int usedBins = 0;
  for (int i = 0; i < iCount; i++)
  {
    if (bins[pcTypes[i]] == 0)
      usedBins++;
    bins[pcTypes[i]]++;
  }

  if (usedBins <= 1)
    return 0.0f;

  float entropy = 0.0f;
  for (int i = 0; i < 256; i++)
  {
    if (bins[i] == 0)
      continue;
    float p = (float)bins[i] / (float)iCount;
    entropy -= p * logf(p);
  }

  float maxEntropy = logf((float)usedBins);
  if (maxEntropy < 0.001f)
    return 0.0f;

  return entropy / maxEntropy;
}

bool HWCuPartitionAnalyzer::hasMotionBoundary(const HWMV* pcMVs,
                                              int iW, int iH,
                                              bool& rbHoriz,
                                              bool& rbVert) const
{
  rbHoriz = false;
  rbVert  = false;
  float threshSq = MV_BOUNDARY_THRESHOLD_PEL * MV_BOUNDARY_THRESHOLD_PEL;

  for (int y = 0; y < iH - 1; y++)
  {
    for (int x = 0; x < iW; x++)
    {
      int idxA = y * iW + x;
      int idxB = (y + 1) * iW + x;
      float distSq = xMVDistSq(pcMVs[idxA], pcMVs[idxB]);
      if (distSq >= threshSq)
        rbHoriz = true;
    }
  }

  for (int y = 0; y < iH; y++)
  {
    for (int x = 0; x < iW - 1; x++)
    {
      int idxA = y * iW + x;
      int idxB = y * iW + (x + 1);
      float distSq = xMVDistSq(pcMVs[idxA], pcMVs[idxB]);
      if (distSq >= threshSq)
        rbVert = true;
    }
  }

  return rbHoriz || rbVert;
}

CUSplitType HWCuPartitionAnalyzer::determineSplitType(
    bool bHorizBoundary, bool bVertBoundary) const
{
  if (bHorizBoundary && bVertBoundary)
    return CU_SPLIT_QT;
  if (bHorizBoundary)
    return CU_SPLIT_BT_H;
  if (bVertBoundary)
    return CU_SPLIT_BT_V;
  return CU_SPLIT_NONE;
}

int HWCuPartitionAnalyzer::extractSubGrid(const MBPartitionGrid& rcGrid,
                                          int iOriginX, int iOriginY,
                                          int iSize,
                                          uint8_t* pcMBTypes,
                                          HWMV* pcMVs,
                                          int& riActualW,
                                          int& riActualH) const
{
  if (iOriginX >= rcGrid.m_iWidth || iOriginY >= rcGrid.m_iHeight)
    return 1;

  riActualW = iSize;
  if (iOriginX + riActualW > rcGrid.m_iWidth)
    riActualW = rcGrid.m_iWidth - iOriginX;

  riActualH = iSize;
  if (iOriginY + riActualH > rcGrid.m_iHeight)
    riActualH = rcGrid.m_iHeight - iOriginY;

  for (int y = 0; y < riActualH; y++)
  {
    for (int x = 0; x < riActualW; x++)
    {
      int srcIdx = (iOriginY + y) * rcGrid.m_iWidth + (iOriginX + x);
      int dstIdx = y * riActualW + x;
      if (srcIdx >= 0 && srcIdx < (int)rcGrid.m_cMBs.size())
      {
        pcMBTypes[dstIdx] = rcGrid.m_cMBs[srcIdx].m_uiMBType;
        pcMVs[dstIdx]     = rcGrid.m_cMBs[srcIdx].m_cMV;
      }
    }
  }

  return 0;
}

float HWCuPartitionAnalyzer::xMVDistSq(const HWMV& rcA, const HWMV& rcB)
{
  float dx = (float)rcA.x - (float)rcB.x;
  float dy = (float)rcA.y - (float)rcB.y;
  return dx * dx + dy * dy;
}

float HWCuPartitionAnalyzer::xMVNormSq(const HWMV& rcMv)
{
  return (float)rcMv.x * (float)rcMv.x + (float)rcMv.y * (float)rcMv.y;
}

float HWCuPartitionAnalyzer::xShannonEntropy(const int* piCounts,
                                             int iNumBins,
                                             int iTotal)
{
  if (iTotal <= 0)
    return 0.0f;
  float entropy = 0.0f;
  for (int i = 0; i < iNumBins; i++)
  {
    if (piCounts[i] == 0)
      continue;
    float p = (float)piCounts[i] / (float)iTotal;
    entropy -= p * logf(p);
  }
  float maxE = logf((float)iNumBins);
  return (maxE > 0.001f) ? (entropy / maxE) : 0.0f;
}

} // namespace vvenc
