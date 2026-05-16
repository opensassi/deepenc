#include "HWBitstreamParser.h"

#include <cstring>
#include <fstream>
#include <sstream>

namespace vvenc {

HWBitstreamParser::HWBitstreamParser()
  : m_bInitialized(false)
  , m_iRawDataBytes(0)
{
}

HWBitstreamParser::~HWBitstreamParser()
{
  release();
}

int HWBitstreamParser::loadFrontmatter(const std::string& cPath,
                                       std::vector<HWFrameMetadata>& rFrames,
                                       int& riWidth,
                                       int& riHeight,
                                       int& riAvgBits)
{
  std::ifstream ifs(cPath);
  if (!ifs.is_open())
    return -1;

  std::string line;

  try
  {
    // Line 1: version,w,h
    if (!std::getline(ifs, line))
      return -2;
    {
      std::stringstream ss(line);
      std::string token;
      int version = 0;
      if (!std::getline(ss, token, ',')) return -2;
      version = std::stoi(token);
      if (version != 1) return -2;
      if (!std::getline(ss, token, ',')) return -2;
      riWidth = std::stoi(token);
      if (!std::getline(ss, token, ',')) return -2;
      riHeight = std::stoi(token);
    }

    // Validate dimensions
    if (riWidth <= 0 || riHeight <= 0)
      return -2;

    // Line 2: header row (poc,frameType,qp,bits,sceneCut,mvComplexity)
    if (!std::getline(ifs, line))
      return -2;

    rFrames.clear();
    uint64_t totalBits = 0;

    // Data rows
    while (std::getline(ifs, line))
    {
      if (line.empty())
        continue;
      std::stringstream ss(line);
      std::string token;
      HWFrameMetadata meta = {};

      if (!std::getline(ss, token, ',')) return -2;
      meta.m_iPOC = std::stoi(token);

      if (!std::getline(ss, token, ',')) return -2;
      meta.m_eFrameType = static_cast<HWFrameType>(std::stoi(token));

      if (!std::getline(ss, token, ',')) return -2;
      meta.m_iQP = std::stoi(token);

      if (!std::getline(ss, token, ',')) return -2;
      meta.m_uBits = std::stoull(token);

      if (!std::getline(ss, token, ',')) return -2;
      meta.m_bSceneCut = (std::stoi(token) != 0);

      if (!std::getline(ss, token, ',')) return -2;
      meta.m_fMVComplexity = std::stof(token);

      totalBits += meta.m_uBits;
      rFrames.push_back(meta);
    }

    riAvgBits = (rFrames.empty()) ? 1 : (int)(totalBits / rFrames.size());
    if (riAvgBits == 0)
      riAvgBits = 1;
  }
  catch (...)
  {
    ifs.close();
    return -2;
  }

  ifs.close();
  m_bInitialized = true;
  return 0;
}

int HWBitstreamParser::loadGridData(const std::string& cPath,
                                    std::vector<HWFrameMetadata>& rFrames,
                                    int iWidth,
                                    int iHeight)
{
  std::ifstream ifs(cPath, std::ios::binary);
  if (!ifs.is_open())
    return -1;

  int gridW = (iWidth  + 15) / 16;
  int gridH = (iHeight + 15) / 16;
  int expectedGridBytes = BIN_HEADER_SIZE + gridW * gridH * (1 + 4); // types(1) + mvPairs(4) per MB


  ifs.seekg(0, std::ios::end);
  int64_t fileSize = ifs.tellg();
  ifs.seekg(0, std::ios::beg);

  if (fileSize <= 0)
  {
    ifs.close();
    return -2;
  }

  m_iRawDataBytes = (int)fileSize;

  std::vector<uint8_t> buffer(fileSize);
  ifs.read(reinterpret_cast<char*>(buffer.data()), fileSize);
  ifs.close();

  int offset = 0;
  for (size_t i = 0; i < rFrames.size(); i++)
  {
    if (offset + 4 > (int)fileSize)
      return -2;

    uint32_t gridSize = 0;
    memcpy(&gridSize, buffer.data() + offset, 4);
    offset += 4;

    if (gridSize != (uint32_t)expectedGridBytes)
      return -2;

    MBPartitionGrid& rcGrid = rFrames[i].m_cMBGrid;
    int consumed = 0;
    int ret = parseGridData(buffer.data() + offset,
                            (int)fileSize - offset,
                            gridW, gridH,
                            rcGrid, consumed);
    if (ret != 0)
      return -2;

    offset += consumed;
  }

  return 0;
}

int HWBitstreamParser::parseGridData(const uint8_t* pBuffer,
                                     int iBufferSz,
                                     int iGridW,
                                     int iGridH,
                                     MBPartitionGrid& rGrid,
                                     int& riConsumed) const
{
  int expectedBytes = iGridW * iGridH * (1 + 4); // types(1) + mvPair(4) per MB
  if (iBufferSz < expectedBytes)
    return -1;

  rGrid.m_iWidth  = iGridW;
  rGrid.m_iHeight = iGridH;
  rGrid.m_cMBs.reserve(iGridW * iGridH);

  const uint8_t*  pTypes = pBuffer;
  // MV data is interleaved int16_t pairs: [mvX0, mvY0, mvX1, mvY1, ...]
  const int16_t*  pMv    = reinterpret_cast<const int16_t*>(pBuffer + iGridW * iGridH);

  for (int y = 0; y < iGridH; y++)
  {
    for (int x = 0; x < iGridW; x++)
    {
      int idx = y * iGridW + x;
      MBPartitionInfo info;
      info.m_iPosX      = x;
      info.m_iPosY      = y;
      info.m_uiMBType   = pTypes[idx];
      info.m_uiSubMBMask = pTypes[idx];
      info.m_cMV.x      = pMv[idx * 2];
      info.m_cMV.y      = pMv[idx * 2 + 1];
      rGrid.m_cMBs.push_back(info);
    }
  }

  riConsumed = expectedBytes;
  return 0;
}

bool HWBitstreamParser::isValidFrameIndex(int iIndex) const
{
  return m_bInitialized && iIndex >= 0;
}

int HWBitstreamParser::release()
{
  m_bInitialized = false;
  m_iRawDataBytes = 0;
  return 0;
}

} // namespace vvenc
