#pragma once

#include "HWPreAnalyzer.h"
#include <cstdint>

namespace vvenc {

class HWCuPartitionAnalyzer
{
public:
  static constexpr float MV_BOUNDARY_THRESHOLD_PEL = 4.0f;
  static constexpr float LOW_MV_VAR_THRESHOLD      = 0.05f;
  static constexpr float LOW_ENTROPY_THRESHOLD      = 0.1f;
  static constexpr float MED_MV_VAR_THRESHOLD       = 0.2f;
  static constexpr float MED_ENTROPY_THRESHOLD      = 0.3f;
  static constexpr float HIGH_MV_VAR_THRESHOLD      = 0.5f;
  static constexpr float HIGH_ENTROPY_THRESHOLD      = 0.6f;

  explicit HWCuPartitionAnalyzer();
  virtual ~HWCuPartitionAnalyzer();

  int computeHint(const MBPartitionGrid& rcGrid,
                  int iCtuX, int iCtuY, int iCUSize,
                  CUSplitHint& rcHint) const;

  float computeMVVariance(const HWMV* pcMVs, int iCount) const;
  float computePartitionEntropy(const uint8_t* pcTypes, int iCount) const;

  bool hasMotionBoundary(const HWMV* pcMVs, int iW, int iH,
                         bool& rbHoriz, bool& rbVert) const;

  CUSplitType determineSplitType(bool bHorizBoundary, bool bVertBoundary) const;

  int extractSubGrid(const MBPartitionGrid& rcGrid,
                     int iOriginX, int iOriginY, int iSize,
                     uint8_t* pcMBTypes, HWMV* pcMVs,
                     int& riActualW, int& riActualH) const;

private:
  static float xMVDistSq(const HWMV& rcA, const HWMV& rcB);
  static float xMVNormSq(const HWMV& rcMv);
  static float xShannonEntropy(const int* piCounts, int iNumBins, int iTotal);
};

} // namespace vvenc
