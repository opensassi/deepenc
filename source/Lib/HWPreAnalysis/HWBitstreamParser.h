#pragma once

#include "HWPreAnalyzer.h"
#include <cstdint>
#include <string>
#include <vector>

namespace vvenc {

class HWBitstreamParser
{
public:
  static constexpr int BIN_HEADER_SIZE = 4;
  static constexpr int MAX_FRAME_COUNT = 65535;

  explicit HWBitstreamParser();
  virtual ~HWBitstreamParser();

  int loadFrontmatter(const std::string& cPath,
                      std::vector<HWFrameMetadata>& rFrames,
                      int& riWidth,
                      int& riHeight,
                      int& riAvgBits);

  int loadGridData(const std::string& cPath,
                   std::vector<HWFrameMetadata>& rFrames,
                   int iWidth,
                   int iHeight);

  int parseGridData(const uint8_t* pBuffer,
                    int iBufferSz,
                    int iGridW,
                    int iGridH,
                    MBPartitionGrid& rGrid,
                    int& riConsumed) const;

  bool isValidFrameIndex(int iIndex) const;
  int  release();

private:
  int xBinarySearchFrame(const std::vector<HWFrameMetadata>& rFrames,
                         int iPOC) const;

  bool m_bInitialized;
  int  m_iRawDataBytes;
};

} // namespace vvenc
