#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace vvenc {

// ── Enums ─────────────────────────────────────────────────────────
enum HWFrameType
{
  HW_FRAME_I       = 0,
  HW_FRAME_P       = 1,
  HW_FRAME_B       = 2,
  HW_FRAME_UNKNOWN = 3
};

enum CUSplitType
{
  CU_SPLIT_NONE = 0,
  CU_SPLIT_QT   = 1,
  CU_SPLIT_BT_H = 2,
  CU_SPLIT_BT_V = 3,
  CU_SPLIT_TT_H = 4,
  CU_SPLIT_TT_V = 5
};

// ── MV helper ─────────────────────────────────────────────────────
struct HWMV
{
  int16_t x;
  int16_t y;
};

// ── Data structures ──────────────────────────────────────────────
struct MBPartitionInfo
{
  int     m_iPosX;
  int     m_iPosY;
  uint8_t m_uiMBType;
  uint8_t m_uiSubMBMask;
  HWMV    m_cMV;
};

struct MBPartitionGrid
{
  int                          m_iWidth;
  int                          m_iHeight;
  std::vector<MBPartitionInfo> m_cMBs;
};

struct HWFrameMetadata
{
  int             m_iPOC;
  HWFrameType     m_eFrameType;
  int             m_iQP;
  uint64_t        m_uBits;
  bool            m_bSceneCut;
  float           m_fMVComplexity;
  MBPartitionGrid m_cMBGrid;
};

struct CUSplitHint
{
  bool        m_bForceSplit;
  bool        m_bNoSplit;
  CUSplitType m_eSplitType;
  float       m_fConfidence;
};

// ── Forward declarations ─────────────────────────────────────────
class HWBitstreamParser;
class HWCuPartitionAnalyzer;

class HWPreAnalyzer
{
public:
  static constexpr int MAX_CTU_SIZE_MB = 8;
  static constexpr int MB_SUB_BLOCK     = 4;

  explicit HWPreAnalyzer();
  virtual ~HWPreAnalyzer();

  static HWPreAnalyzer* getInstance();
  static void setInstance(HWPreAnalyzer* pInstance);

  int  init(const std::string& cMetadataPath);
  int  uninit();
  bool isInitialized() const;

  int getFrameMetadata(int iPOC, const HWFrameMetadata*& ppcMeta) const;
  int getFrameComplexity(int iPOC, float& rfComplexity) const;
  int getSceneCut(int iPOC, bool& rbSceneCut) const;
  int getCUSplitHint(int iCtuX, int iCtuY, int iCUSize,
                     CUSplitHint& rcHint) const;
  int getMVField(int iPOC, const HWMV*& ppcMVGrid,
                 int& riGridW, int& riGridH) const;

  int getNumFrames() const;

private:
  int  xLoadCSV(const std::string& cPath);
  int  xLoadGridData(const std::string& cBinPath);
  int  xFindFrameIndex(int iPOC) const;

  bool                          m_bInitialized;
  std::vector<HWFrameMetadata>  m_cFrameMetadata;
  HWBitstreamParser*            m_pParser;
  HWCuPartitionAnalyzer*        m_pCuAnalyzer;
  int                           m_iWidth;
  int                           m_iHeight;
  int                           m_iNumFrames;
  int                           m_iAvgBits;

  static HWPreAnalyzer* s_pInstance;
};

} // namespace vvenc
