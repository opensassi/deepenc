#include "HWPreAnalyzer.h"
#include "HWBitstreamParser.h"
#include "HWCuPartitionAnalyzer.h"

namespace vvenc {

HWPreAnalyzer* HWPreAnalyzer::s_pInstance = nullptr;

HWPreAnalyzer::HWPreAnalyzer()
  : m_bInitialized(false)
  , m_pParser(nullptr)
  , m_pCuAnalyzer(nullptr)
  , m_iWidth(0)
  , m_iHeight(0)
  , m_iNumFrames(0)
  , m_iAvgBits(0)
{
}

HWPreAnalyzer::~HWPreAnalyzer()
{
  uninit();
}

HWPreAnalyzer* HWPreAnalyzer::getInstance()
{
  return s_pInstance;
}

void HWPreAnalyzer::setInstance(HWPreAnalyzer* pInstance)
{
  s_pInstance = pInstance;
}

int HWPreAnalyzer::init(const std::string& cMetadataPath)
{
  if (m_bInitialized)
    return -1;

  m_pParser = new HWBitstreamParser();
  m_pCuAnalyzer = new HWCuPartitionAnalyzer();

  int ret = xLoadCSV(cMetadataPath);
  if (ret != 0)
  {
    delete m_pParser;     m_pParser = nullptr;
    delete m_pCuAnalyzer; m_pCuAnalyzer = nullptr;
    return ret;
  }

  std::string cBinPath = cMetadataPath;
  size_t pos = cBinPath.rfind('.');
  if (pos != std::string::npos)
    cBinPath = cBinPath.substr(0, pos) + "_grids.bin";
  else
    cBinPath += "_grids.bin";

  ret = xLoadGridData(cBinPath);
  if (ret != 0)
  {
    delete m_pParser;     m_pParser = nullptr;
    delete m_pCuAnalyzer; m_pCuAnalyzer = nullptr;
    return -3;
  }

  m_iNumFrames = (int)m_cFrameMetadata.size();
  m_bInitialized = true;
  return 0;
}

int HWPreAnalyzer::uninit()
{
  m_cFrameMetadata.clear();
  if (m_pParser)
  {
    m_pParser->release();
    delete m_pParser;
    m_pParser = nullptr;
  }
  if (m_pCuAnalyzer)
  {
    delete m_pCuAnalyzer;
    m_pCuAnalyzer = nullptr;
  }
  m_bInitialized = false;
  m_iWidth = 0;
  m_iHeight = 0;
  m_iNumFrames = 0;
  m_iAvgBits = 0;
  return 0;
}

bool HWPreAnalyzer::isInitialized() const
{
  return m_bInitialized;
}

int HWPreAnalyzer::xLoadCSV(const std::string& cPath)
{
  return m_pParser->loadFrontmatter(cPath, m_cFrameMetadata,
                                    m_iWidth, m_iHeight, m_iAvgBits);
}

int HWPreAnalyzer::xLoadGridData(const std::string& cBinPath)
{
  return m_pParser->loadGridData(cBinPath, m_cFrameMetadata,
                                 m_iWidth, m_iHeight);
}

int HWPreAnalyzer::getFrameMetadata(int iPOC,
                                    const HWFrameMetadata*& ppcMeta) const
{
  if (!m_bInitialized)
    return 1;
  int idx = xFindFrameIndex(iPOC);
  if (idx < 0)
    return 1;
  ppcMeta = &m_cFrameMetadata[idx];
  return 0;
}

int HWPreAnalyzer::getFrameComplexity(int iPOC, float& rfComplexity) const
{
  if (!m_bInitialized || m_iAvgBits == 0)
    return 1;
  int idx = xFindFrameIndex(iPOC);
  if (idx < 0)
    return 1;
  rfComplexity = (float)m_cFrameMetadata[idx].m_uBits / (float)m_iAvgBits;
  return 0;
}

int HWPreAnalyzer::getSceneCut(int iPOC, bool& rbSceneCut) const
{
  if (!m_bInitialized)
    return 1;
  int idx = xFindFrameIndex(iPOC);
  if (idx < 0)
    return 1;
  rbSceneCut = m_cFrameMetadata[idx].m_bSceneCut;
  return 0;
}

int HWPreAnalyzer::getCUSplitHint(int iCtuX, int iCtuY, int iCUSize,
                                  CUSplitHint& rcHint) const
{
  if (!m_bInitialized || !m_pCuAnalyzer)
    return 1;
  if (m_cFrameMetadata.empty())
    return 1;
  const MBPartitionGrid& rcGrid = m_cFrameMetadata[0].m_cMBGrid;
  return m_pCuAnalyzer->computeHint(rcGrid, iCtuX, iCtuY, iCUSize, rcHint);
}

int HWPreAnalyzer::getMVField(int iPOC, const HWMV*& ppcMVGrid,
                              int& riGridW, int& riGridH) const
{
  if (!m_bInitialized)
    return 1;
  int idx = xFindFrameIndex(iPOC);
  if (idx < 0)
    return 1;
  const MBPartitionGrid& rcGrid = m_cFrameMetadata[idx].m_cMBGrid;
  if (rcGrid.m_cMBs.empty())
    return 1;
  riGridW = rcGrid.m_iWidth;
  riGridH = rcGrid.m_iHeight;
  ppcMVGrid = &rcGrid.m_cMBs[0].m_cMV;
  return 0;
}

int HWPreAnalyzer::getNumFrames() const
{
  return m_iNumFrames;
}

int HWPreAnalyzer::xFindFrameIndex(int iPOC) const
{
  for (int i = 0; i < (int)m_cFrameMetadata.size(); i++)
  {
    if (m_cFrameMetadata[i].m_iPOC == iPOC)
      return i;
  }
  return -1;
}

} // namespace vvenc
