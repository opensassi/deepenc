/** \file     hw_preanalysis_test.cpp
    \brief    HWPreAnalysis module unit tests
*/

#include "HWPreAnalyzer.h"
#include "HWBitstreamParser.h"
#include "HWCuPartitionAnalyzer.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace vvenc;

// ── Test macros ───────────────────────────────────────────────────
#define TEST(x)   { int res = (x); g_numTests++; g_numFails += (res != 0); }
#define TESTT(x,w){ int res = (x); g_numTests++; g_numFails += (res != 0); }
#define ERROR(w)  { g_numTests++; g_numFails++; }

static int g_numTests = 0;
static int g_numFails = 0;

// ── Assertion helpers ────────────────────────────────────────────
static int g_numAsserts = 0;
static int g_numFailsAsserts = 0;

#define CHECK_EQ(a, b, msg) \
  do { \
    g_numAsserts++; \
    if ((a) != (b)) { \
      std::cerr << "FAIL [" << msg << "]: expected " << (b) << " got " << (a) << std::endl; \
      g_numFailsAsserts++; \
      return 1; \
    } \
  } while (0)

#define CHECK_NEAR(a, b, eps, msg) \
  do { \
    g_numAsserts++; \
    float diff = std::fabs((float)(a) - (float)(b)); \
    if (diff > (eps)) { \
      std::cerr << "FAIL [" << msg << "]: expected ~" << (b) << " got " << (a) << " (diff=" << diff << ")" << std::endl; \
      g_numFailsAsserts++; \
      return 1; \
    } \
  } while (0)

#define CHECK_TRUE(cond, msg) \
  do { \
    g_numAsserts++; \
    if (!(cond)) { \
      std::cerr << "FAIL [" << msg << "]: expected true" << std::endl; \
      g_numFailsAsserts++; \
      return 1; \
    } \
  } while (0)

#define CHECK_FALSE(cond, msg) \
  do { \
    g_numAsserts++; \
    if ((cond)) { \
      std::cerr << "FAIL [" << msg << "]: expected false" << std::endl; \
      g_numFailsAsserts++; \
      return 1; \
    } \
  } while (0)

// ── Helpers ──────────────────────────────────────────────────────
static std::string createTempCSV(const std::string& content)
{
  char tmpl[] = "/tmp/hw_test_XXXXXX";
  int fd = mkstemp(tmpl);
  if (fd < 0) return "";
  close(fd);
  std::string oldPath(tmpl);
  std::string newPath = oldPath + ".csv";
  std::rename(oldPath.c_str(), newPath.c_str());
  std::ofstream of(newPath);
  of << content;
  of.flush();
  of.close();
  return newPath;
}

static MBPartitionGrid makeUniformGrid(int w, int h, uint8_t mbType,
                                       int16_t mvX, int16_t mvY)
{
  MBPartitionGrid grid;
  grid.m_iWidth  = w;
  grid.m_iHeight = h;
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++)
    {
      MBPartitionInfo info;
      info.m_iPosX     = x;
      info.m_iPosY     = y;
      info.m_uiMBType  = mbType;
      info.m_uiSubMBMask = mbType;
      info.m_cMV.x     = mvX;
      info.m_cMV.y     = mvY;
      grid.m_cMBs.push_back(info);
    }
  return grid;
}

static MBPartitionGrid makeMixedGrid(int w, int h,
                                     uint8_t tL, uint8_t tR,
                                     int16_t mvLX, int16_t mvLY,
                                     int16_t mvRX, int16_t mvRY)
{
  MBPartitionGrid grid;
  grid.m_iWidth  = w;
  grid.m_iHeight = h;
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++)
    {
      MBPartitionInfo info;
      info.m_iPosX = x;
      info.m_iPosY = y;
      if (x < w / 2)
      {
        info.m_uiMBType = tL;
        info.m_cMV.x    = mvLX;
        info.m_cMV.y    = mvLY;
      }
      else
      {
        info.m_uiMBType = tR;
        info.m_cMV.x    = mvRX;
        info.m_cMV.y    = mvRY;
      }
      info.m_uiSubMBMask = info.m_uiMBType;
      grid.m_cMBs.push_back(info);
    }
  return grid;
}

// ===================================================================
// Test IDs
// ===================================================================
enum TestId
{
  // Bitstream parser
  HW_BITSTREAM_LOAD_FRONTMATTER      = 1,
  HW_BITSTREAM_MISSING_FILE          = 2,
  HW_BITSTREAM_CORRUPT_CSV           = 3,
  HW_BITSTREAM_MISSING_FIELDS        = 4,
  HW_BITSTREAM_ZERO_FRAMES           = 5,
  HW_BITSTREAM_RELEASE               = 6,
  HW_BITSTREAM_PARSE_GRID            = 7,
  HW_BITSTREAM_PARSE_TRUNCATED       = 8,
  // MV variance
  HW_MV_VARIANCE_ZERO                = 10,
  HW_MV_VARIANCE_HIGH                = 11,
  HW_MV_VARIANCE_SINGLE              = 12,
  // Partition entropy
  HW_PARTITION_ENTROPY_UNIFORM       = 15,
  HW_PARTITION_ENTROPY_MIXED         = 16,
  HW_PARTITION_ENTROPY_TWO           = 17,
  // Motion boundary
  HW_MOTION_BOUNDARY_8PEL            = 20,
  HW_MOTION_BOUNDARY_4PEL            = 21,
  HW_MOTION_BOUNDARY_1PEL            = 22,
  HW_MOTION_BOUNDARY_IDENTICAL       = 23,
  // CU split hint
  HW_CU_HINT_HOMOGENEOUS_NO_SPLIT    = 30,
  HW_CU_HINT_BOUNDARY_HORIZ          = 31,
  HW_CU_HINT_BOUNDARY_VERT           = 32,
  HW_CU_HINT_BOUNDARY_BOTH           = 33,
  HW_CU_HINT_MEDIUM_VARIANCE         = 34,
  HW_CU_HINT_HIGH_VARIANCE           = 35,
  HW_CU_HINT_HIGH_ENTROPY            = 36,
  HW_CU_HINT_LOW_CONFIDENCE          = 37,
  HW_CU_HINT_INSUFFICIENT_DATA       = 38,
  HW_CU_HINT_FRAME_BOUNDARY          = 39,
  // Sub-grid extraction
  HW_EXTRACT_SUBGRID_FULL            = 45,
  HW_EXTRACT_SUBGRID_TRUNCATED       = 46,
  HW_EXTRACT_SUBGRID_OUT_OF_BOUNDS   = 47,
  // Split type determination
  HW_DETERMINE_SPLIT_HORIZ           = 50,
  HW_DETERMINE_SPLIT_VERT            = 51,
  HW_DETERMINE_SPLIT_BOTH            = 52,
  HW_DETERMINE_SPLIT_NEITHER         = 53,
  // Facade lifecycle
  HW_INIT_LOAD                       = 60,
  HW_INIT_MISSING_FILE               = 61,
  HW_INIT_CORRUPT_JSON               = 62,
  HW_INIT_MISSING_BIN                = 63,
  HW_GET_FRAME_META                  = 64,
  HW_GET_FRAME_META_INVALID          = 65,
  HW_GET_COMPLEXITY                  = 66,
  HW_GET_SCENE_CUT                   = 67,
  HW_GET_MV_FIELD                    = 68,
  HW_GET_MV_FIELD_INVALID            = 69,
  HW_GET_CU_HINT_NO_SPLIT            = 70,
  HW_GET_CU_HINT_FORCE_SPLIT         = 71,
  HW_GET_CU_HINT_LOW_CONF            = 72,
  HW_GET_CU_HINT_INSUFFICIENT        = 73,
  HW_LIFECYCLE_UNINIT_TWICE          = 74,
};

// ===================================================================
// Test functions
// ===================================================================

static int testBitstreamLoadFrontmatter()
{
  std::string csv = createTempCSV(
    "1,832,480\n"
    "poc,frameType,qp,bits,sceneCut,mvComplexity\n"
    "0,0,28,123456,1,0.75\n"
    "1,1,32,54321,0,0.31\n"
    "2,2,30,23456,0,0.42\n"
  );

  HWBitstreamParser parser;
  std::vector<HWFrameMetadata> frames;
  int w = 0, h = 0, avg = 0;
  int ret = parser.loadFrontmatter(csv, frames, w, h, avg);
  CHECK_EQ(ret, 0, "load frontmatter OK");
  CHECK_EQ(w, 832, "width OK");
  CHECK_EQ(h, 480, "height OK");
  CHECK_EQ((int)frames.size(), 3, "frame count OK");
  CHECK_EQ(frames[0].m_iPOC, 0, "POC 0");
  CHECK_EQ(frames[0].m_eFrameType, HW_FRAME_I, "frame type I");
  CHECK_EQ(frames[0].m_iQP, 28, "QP 28");
  CHECK_EQ(frames[0].m_uBits, (uint64_t)123456, "bits 123456");
  CHECK_TRUE(frames[0].m_bSceneCut, "scene cut true");
  CHECK_EQ(frames[1].m_iPOC, 1, "POC 1");
  CHECK_EQ(frames[1].m_eFrameType, HW_FRAME_P, "frame type P");
  CHECK_EQ(frames[2].m_eFrameType, HW_FRAME_B, "frame type B");
  CHECK_TRUE(avg > 0, "avg bits > 0");
  std::remove(csv.c_str());
  return 0;
}

static int testBitstreamMissingFile()
{
  HWBitstreamParser parser;
  std::vector<HWFrameMetadata> frames;
  int w = 0, h = 0, avg = 0;
  int ret = parser.loadFrontmatter("/nonexistent/path.csv", frames, w, h, avg);
  CHECK_EQ(ret, -1, "missing file returns -1");
  return 0;
}

static int testBitstreamCorruptCSV()
{
  // Version line must parse as ints, missing header causes -2
  std::string csv = createTempCSV("x,y,z\n");
  HWBitstreamParser parser;
  std::vector<HWFrameMetadata> frames;
  int w = 0, h = 0, avg = 0;
  int ret = parser.loadFrontmatter(csv, frames, w, h, avg);
  CHECK_EQ(ret, -2, "corrupt CSV returns -2");
  std::remove(csv.c_str());
  return 0;
}

static int testBitstreamZeroFrames()
{
  std::string csv = createTempCSV(
    "1,832,480\n"
    "poc,frameType,qp,bits,sceneCut,mvComplexity\n"
  );
  HWBitstreamParser parser;
  std::vector<HWFrameMetadata> frames;
  int w = 0, h = 0, avg = 0;
  int ret = parser.loadFrontmatter(csv, frames, w, h, avg);
  CHECK_EQ(ret, 0, "zero frames OK");
  CHECK_EQ((int)frames.size(), 0, "no frames");
  std::remove(csv.c_str());
  return 0;
}

static int testBitstreamRelease()
{
  HWBitstreamParser parser;
  CHECK_EQ(parser.release(), 0, "release OK");
  CHECK_FALSE(parser.isValidFrameIndex(0), "invalid after release");
  return 0;
}

static int testBitstreamParseGrid()
{
  int W = 4, H = 4;
  // Interleaved format: types(W*H) + mvPairs (int16_t mvX, mvY per MB)
  std::vector<uint8_t> buffer(W * H + W * H * 2 * sizeof(int16_t));
  uint8_t* pTypes = buffer.data();
  int16_t* pMv    = reinterpret_cast<int16_t*>(buffer.data() + W * H);
  for (int i = 0; i < W * H; i++)
  {
    pTypes[i]    = (uint8_t)(i % 4);
    pMv[i * 2]     = (int16_t)(i);
    pMv[i * 2 + 1] = (int16_t)(-i);
  }

  HWBitstreamParser parser;
  MBPartitionGrid grid;
  int consumed = 0;
  int ret = parser.parseGridData(buffer.data(), (int)buffer.size(),
                                 W, H, grid, consumed);
  CHECK_EQ(ret, 0, "parse grid OK");
  CHECK_EQ(grid.m_iWidth, W, "grid W");
  CHECK_EQ(grid.m_iHeight, H, "grid H");
  CHECK_EQ(grid.m_cMBs[0].m_uiMBType, pTypes[0], "type[0]");
  CHECK_EQ(grid.m_cMBs[0].m_cMV.x, pMv[0], "mvX[0]");
  CHECK_EQ(grid.m_cMBs[0].m_cMV.y, pMv[1], "mvY[0]");
  CHECK_EQ(grid.m_cMBs[W*H-1].m_uiMBType, pTypes[W*H-1], "type last");
  CHECK_EQ(consumed, W * H * (1 + 4), "consumed bytes");
  return 0;
}

static int testBitstreamParseTruncated()
{
  HWBitstreamParser parser;
  uint8_t buf[10] = {0};
  MBPartitionGrid grid;
  int consumed = 0;
  int ret = parser.parseGridData(buf, 10, 8, 8, grid, consumed);
  CHECK_EQ(ret, -1, "truncated buffer returns -1");
  return 0;
}

// ── MV Variance ──────────────────────────────────────────────────

static int testMVVarianceZero()
{
  HWCuPartitionAnalyzer ana;
  HWMV mvs[16];
  for (int i = 0; i < 16; i++) mvs[i] = {5, -3};
  float v = ana.computeMVVariance(mvs, 16);
  CHECK_NEAR(v, 0.0f, 0.0001f, "identical MVs -> 0.0");
  return 0;
}

static int testMVVarianceHigh()
{
  HWCuPartitionAnalyzer ana;
  HWMV mvs[16];
  for (int i = 0; i < 16; i++)
  {
    mvs[i].x = (i % 2 == 0) ? -30000 : 30000;
    mvs[i].y = (i % 2 == 0) ? -30000 : 30000;
  }
  float v = ana.computeMVVariance(mvs, 16);
  CHECK_TRUE(v > 0.5f, "alternating MVs -> high variance");
  return 0;
}

static int testMVVarianceSingle()
{
  HWCuPartitionAnalyzer ana;
  HWMV mvs[1] = {{42, -17}};
  float v = ana.computeMVVariance(mvs, 1);
  CHECK_NEAR(v, 0.0f, 0.0001f, "single MV -> 0.0");
  return 0;
}

// ── Partition Entropy ────────────────────────────────────────────

static int testEntropyUniform()
{
  HWCuPartitionAnalyzer ana;
  uint8_t types[16];
  memset(types, 0, 16);
  float e = ana.computePartitionEntropy(types, 16);
  CHECK_NEAR(e, 0.0f, 0.0001f, "uniform -> 0.0");
  return 0;
}

static int testEntropyMixed()
{
  HWCuPartitionAnalyzer ana;
  uint8_t types[8];
  for (int i = 0; i < 8; i++) types[i] = (uint8_t)i;
  float e = ana.computePartitionEntropy(types, 8);
  CHECK_NEAR(e, 1.0f, 0.01f, "all distinct -> ~1.0");
  return 0;
}

static int testEntropyTwo()
{
  HWCuPartitionAnalyzer ana;
  uint8_t types[8];
  for (int i = 0; i < 4; i++) types[i] = 0;
  for (int i = 4; i < 8; i++) types[i] = 1;
  float e = ana.computePartitionEntropy(types, 8);
  CHECK_NEAR(e, 1.0f, 0.01f, "two types even split -> 1.0");
  return 0;
}

// ── Motion Boundary ──────────────────────────────────────────────

static int testBoundary8pel()
{
  HWCuPartitionAnalyzer ana;
  HWMV mvs[4] = {{0,0},{0,0},{8,0},{8,0}};
  bool h = false, v = false;
  CHECK_TRUE(ana.hasMotionBoundary(mvs, 2, 2, h, v), "8pel diff -> boundary");
  return 0;
}

static int testBoundary4pel()
{
  HWCuPartitionAnalyzer ana;
  HWMV mvs[4] = {{0,0},{0,0},{4,0},{4,0}};
  bool h = false, v = false;
  CHECK_TRUE(ana.hasMotionBoundary(mvs, 2, 2, h, v), "4pel diff -> boundary (inclusive)");
  return 0;
}

static int testBoundary1pel()
{
  HWCuPartitionAnalyzer ana;
  HWMV mvs[4] = {{0,0},{0,0},{1,0},{1,0}};
  bool h = false, v = false;
  CHECK_FALSE(ana.hasMotionBoundary(mvs, 2, 2, h, v), "1pel diff -> no boundary");
  return 0;
}

static int testBoundaryIdentical()
{
  HWCuPartitionAnalyzer ana;
  HWMV mvs[4] = {{3,3},{3,3},{3,3},{3,3}};
  bool h = false, v = false;
  CHECK_FALSE(ana.hasMotionBoundary(mvs, 2, 2, h, v), "identical -> no boundary");
  return 0;
}

// ── CU Split Hint ────────────────────────────────────────────────

static int testHintHomogeneousNoSplit()
{
  HWCuPartitionAnalyzer ana;
  auto grid = makeUniformGrid(8, 8, 0, 0, 0);
  CUSplitHint hint;
  int ret = ana.computeHint(grid, 0, 0, 8, hint);
  CHECK_EQ(ret, 0, "noSplit return OK");
  CHECK_TRUE(hint.m_bNoSplit, "hint noSplit");
  CHECK_TRUE(hint.m_fConfidence > 0.8f, "confidence > 0.8");
  return 0;
}

static int testHintBoundaryHoriz()
{
  // Left/right grid split -> vertical MV boundary -> BT_V
  HWCuPartitionAnalyzer ana;
  auto grid = makeMixedGrid(8, 8, 0, 1, 0, 0, 10, 0);
  CUSplitHint hint;
  int ret = ana.computeHint(grid, 0, 0, 8, hint);
  CHECK_EQ(ret, 0, "boundary return OK");
  CHECK_TRUE(hint.m_bForceSplit, "hint forceSplit");
  CHECK_EQ(hint.m_eSplitType, CU_SPLIT_BT_V, "split BT_V");
  CHECK_TRUE(hint.m_fConfidence > 0.7f, "confidence > 0.7");
  return 0;
}

static int testHintBoundaryVert()
{
  // Top/bottom grid split -> horizontal MV boundary -> BT_H
  // Top half: mv=(0,0), bottom half: mv=(0,10)
  HWCuPartitionAnalyzer ana;
  MBPartitionGrid grid;
  grid.m_iWidth = 8; grid.m_iHeight = 8;
  for (int y = 0; y < 8; y++)
    for (int x = 0; x < 8; x++)
    {
      MBPartitionInfo info;
      info.m_iPosX = x; info.m_iPosY = y;
      info.m_uiMBType = 0; info.m_uiSubMBMask = 0;
      info.m_cMV.x = 0;
      info.m_cMV.y = (y < 4) ? 0 : 10;
      grid.m_cMBs.push_back(info);
    }
  CUSplitHint hint;
  int ret = ana.computeHint(grid, 0, 0, 8, hint);
  CHECK_EQ(ret, 0, "boundary vert return OK");
  CHECK_TRUE(hint.m_bForceSplit, "hint forceSplit");
  CHECK_EQ(hint.m_eSplitType, CU_SPLIT_BT_H, "split BT_H");
  return 0;
}

static int testHintBoundaryBoth()
{
  // Top/bottom + left/right split -> both boundaries -> QT
  HWCuPartitionAnalyzer ana;
  MBPartitionGrid grid;
  grid.m_iWidth = 8; grid.m_iHeight = 8;
  for (int y = 0; y < 8; y++)
    for (int x = 0; x < 8; x++)
    {
      MBPartitionInfo info;
      info.m_iPosX = x; info.m_iPosY = y;
      info.m_uiMBType = (x >= 4 || y >= 4) ? 1 : 0;
      info.m_uiSubMBMask = info.m_uiMBType;
      info.m_cMV.x = (x >= 4) ? 10 : 0;
      info.m_cMV.y = (y >= 4) ? 10 : 0;
      grid.m_cMBs.push_back(info);
    }
  CUSplitHint hint;
  int ret = ana.computeHint(grid, 0, 0, 8, hint);
  CHECK_EQ(ret, 0, "boundary both OK");
  CHECK_TRUE(hint.m_bForceSplit, "forceSplit");
  CHECK_EQ(hint.m_eSplitType, CU_SPLIT_QT, "split QT");
  return 0;
}

static int testHintMediumVariance()
{
  // Identical MVs (no boundary), entropy from 3 type-1 cells among 61 type-0: ~0.16
  // mvVar=0 < LOW but entropy=0.16 > LOW (0.1) -> skip branch 2
  // mvVar < MED (0 < 0.2) AND entropy < MED (0.16 < 0.3) -> branch 3: conf=0.6
  HWCuPartitionAnalyzer ana;
  MBPartitionGrid grid;
  grid.m_iWidth = 8; grid.m_iHeight = 8;
  for (int y = 0; y < 8; y++)
    for (int x = 0; x < 8; x++) {
      MBPartitionInfo info;
      info.m_iPosX = x; info.m_iPosY = y;
      int i = y * 8 + x;
      info.m_uiMBType = (i < 61) ? 0 : 1;
      info.m_uiSubMBMask = info.m_uiMBType;
      info.m_cMV.x = 0;
      info.m_cMV.y = 0;
      grid.m_cMBs.push_back(info);
    }
  CUSplitHint hint;
  int ret = ana.computeHint(grid, 0, 0, 8, hint);
  CHECK_EQ(ret, 0, "medium return OK");
  CHECK_TRUE(hint.m_bNoSplit, "noSplit");
  CHECK_NEAR(hint.m_fConfidence, 0.6f, 0.05f, "medium -> confidence 0.6");
  return 0;
}

static int testHintHighVariance()
{
  // mv.x alternates by column (x%2), mv.y alternates by row (y%2)
  // → both horizontal and vertical boundaries detected → QT + conf=0.8
  HWCuPartitionAnalyzer ana;
  MBPartitionGrid grid;
  grid.m_iWidth = 8; grid.m_iHeight = 8;
  for (int y = 0; y < 8; y++)
    for (int x = 0; x < 8; x++) {
      MBPartitionInfo info;
      info.m_iPosX = x; info.m_iPosY = y;
      info.m_uiMBType = 0; info.m_uiSubMBMask = 0;
      info.m_cMV.x = (x % 2 == 0) ? -30000 : 30000;
      info.m_cMV.y = (y % 2 == 0) ? -30000 : 30000;
      grid.m_cMBs.push_back(info);
    }
  CUSplitHint hint;
  int ret = ana.computeHint(grid, 0, 0, 8, hint);
  CHECK_EQ(ret, 0, "high var return OK");
  CHECK_TRUE(hint.m_bForceSplit, "forceSplit");
  CHECK_EQ(hint.m_eSplitType, CU_SPLIT_QT, "split QT");
  CHECK_NEAR(hint.m_fConfidence, 0.92f, 0.01f, "boundary first -> conf 0.92");
  return 0;
}

static int testHintHighEntropy()
{
  HWCuPartitionAnalyzer ana;
  auto grid = makeUniformGrid(8, 8, 0, 0, 0);
  for (int i = 0; i < 64 && i < (int)grid.m_cMBs.size(); i++)
    grid.m_cMBs[i].m_uiMBType = (uint8_t)(i % 16);
  CUSplitHint hint;
  int ret = ana.computeHint(grid, 0, 0, 8, hint);
  CHECK_EQ(ret, 0, "high entropy return OK");
  CHECK_TRUE(hint.m_bForceSplit, "forceSplit");
  CHECK_EQ(hint.m_eSplitType, CU_SPLIT_QT, "split QT");
  return 0;
}

static int testHintLowConfidence()
{
  // Identical MVs (no boundary), entropy ~0.38 from 12 type-1 among 52 type-0.
  // mvVar=0 < LOW (0.05) but entropy=0.38 > LOW (0.1) -> skip branch 2
  // mvVar < MED (0 < 0.2) but entropy=0.38 > MED (0.3) -> skip branch 3
  // mvVar < HIGH (0 < 0.5) AND entropy < HIGH (0.38 < 0.6) -> skip branch 4
  // Falls to else -> conf=0.3
  HWCuPartitionAnalyzer ana;
  MBPartitionGrid grid;
  grid.m_iWidth = 8; grid.m_iHeight = 8;
  for (int y = 0; y < 8; y++)
    for (int x = 0; x < 8; x++) {
      MBPartitionInfo info;
      info.m_iPosX = x; info.m_iPosY = y;
      int i = y * 8 + x;
      info.m_uiMBType = (i < 55) ? 0 : 1;
      info.m_uiSubMBMask = info.m_uiMBType;
      info.m_cMV.x = 0;
      info.m_cMV.y = 0;
      grid.m_cMBs.push_back(info);
    }
  CUSplitHint hint;
  int ret = ana.computeHint(grid, 0, 0, 8, hint);
  CHECK_EQ(ret, 0, "low conf return OK");
  CHECK_NEAR(hint.m_fConfidence, 0.3f, 0.02f, "low conf -> 0.3");
  return 0;
}

static int testHintInsufficientData()
{
  HWCuPartitionAnalyzer ana;
  auto grid = makeUniformGrid(4, 4, 0, 0, 0);
  CUSplitHint hint;
  int ret = ana.computeHint(grid, 2, 2, 8, hint);
  CHECK_EQ(ret, 1, "insufficient data returns 1");
  return 0;
}

static int testHintFrameBoundary()
{
  HWCuPartitionAnalyzer ana;
  auto grid = makeUniformGrid(10, 8, 0, 0, 0);
  CUSplitHint hint;
  int ret = ana.computeHint(grid, 0, 0, 8, hint);
  CHECK_EQ(ret, 0, "frame boundary return OK");
  CHECK_TRUE(hint.m_bNoSplit, "noSplit at uniform boundary");
  CHECK_TRUE(hint.m_fConfidence > 0.0f, "confidence positive");
  return 0;
}

// ── Sub-grid Extraction ──────────────────────────────────────────

static int testExtractSubgridFull()
{
  HWCuPartitionAnalyzer ana;
  auto grid = makeUniformGrid(16, 16, 42, 7, -3);
  uint8_t types[64];
  HWMV mvs[64];
  int aw = 0, ah = 0;
  int ret = ana.extractSubGrid(grid, 4, 4, 8, types, mvs, aw, ah);
  CHECK_EQ(ret, 0, "extract full OK");
  CHECK_EQ(aw, 8, "actual w = 8");
  CHECK_EQ(ah, 8, "actual h = 8");
  CHECK_EQ(types[0], 42, "type copied");
  CHECK_EQ(mvs[0].x, 7, "mv x copied");
  return 0;
}

static int testExtractSubgridTruncated()
{
  HWCuPartitionAnalyzer ana;
  auto grid = makeUniformGrid(10, 16, 0, 0, 0);
  uint8_t types[64];
  HWMV mvs[64];
  int aw = 0, ah = 0;
  int ret = ana.extractSubGrid(grid, 8, 0, 8, types, mvs, aw, ah);
  CHECK_EQ(ret, 0, "extract truncated OK");
  CHECK_EQ(aw, 2, "truncated to 2 cols");
  CHECK_EQ(ah, 8, "full 8 rows");
  return 0;
}

static int testExtractSubgridOutOfBounds()
{
  HWCuPartitionAnalyzer ana;
  auto grid = makeUniformGrid(8, 8, 0, 0, 0);
  uint8_t types[4]; HWMV mvs[4];
  int aw = 0, ah = 0;
  int ret = ana.extractSubGrid(grid, 20, 20, 2, types, mvs, aw, ah);
  CHECK_EQ(ret, 1, "out of bounds returns 1");
  return 0;
}

// ── Split Type Determination ─────────────────────────────────────

static int testSplitHoriz()
{
  HWCuPartitionAnalyzer ana;
  CHECK_EQ(ana.determineSplitType(true, false), CU_SPLIT_BT_H, "h-only -> BT_H");
  return 0;
}

static int testSplitVert()
{
  HWCuPartitionAnalyzer ana;
  CHECK_EQ(ana.determineSplitType(false, true), CU_SPLIT_BT_V, "v-only -> BT_V");
  return 0;
}

static int testSplitBoth()
{
  HWCuPartitionAnalyzer ana;
  CHECK_EQ(ana.determineSplitType(true, true), CU_SPLIT_QT, "both -> QT");
  return 0;
}

static int testSplitNeither()
{
  HWCuPartitionAnalyzer ana;
  CHECK_EQ(ana.determineSplitType(false, false), CU_SPLIT_NONE, "neither -> NONE");
  return 0;
}

// ── Facade Lifecycle ─────────────────────────────────────────────

static int testInitLoad()
{
  std::string csv = createTempCSV(
    "1,832,480\n"
    "poc,frameType,qp,bits,sceneCut,mvComplexity\n"
    "0,0,28,123456,1,0.75\n"
  );
  std::string binPath = csv.substr(0, csv.rfind('.')) + "_grids.bin";
  {
    int W = 52, H = 30;
    int frameBytes = 4 + W * H * 5;
    uint32_t gs = (uint32_t)frameBytes;
    std::vector<uint8_t> frameBuf(frameBytes);
    memcpy(frameBuf.data(), &gs, 4);
    memset(frameBuf.data() + 4, 0, frameBytes - 4);
    std::ofstream of(binPath, std::ios::binary);
    of.write((const char*)frameBuf.data(), frameBuf.size());
    of.close();
  }
  HWPreAnalyzer ana;
  int ret = ana.init(csv);
  CHECK_EQ(ret, 0, "init OK");
  CHECK_TRUE(ana.isInitialized(), "isInitialized true");
  ana.uninit();
  std::remove(csv.c_str());
  std::remove(binPath.c_str());
  return 0;
}

static int testInitMissingFile()
{
  HWPreAnalyzer ana;
  int ret = ana.init("/nonexistent.csv");
  CHECK_EQ(ret, -1, "missing file -> -1");
  return 0;
}

static int testInitMissingBin()
{
  std::string csv = createTempCSV(
    "1,832,480\n"
    "poc,frameType,qp,bits,sceneCut,mvComplexity\n"
    "0,0,28,123456,1,0.75\n"
  );
  HWPreAnalyzer ana;
  int ret = ana.init(csv);
  CHECK_EQ(ret, -3, "missing bin -> -3");
  std::remove(csv.c_str());
  return 0;
}

static int testGetFrameMeta()
{
  std::string csv = createTempCSV(
    "1,832,480\n"
    "poc,frameType,qp,bits,sceneCut,mvComplexity\n"
    "0,0,28,123456,1,0.75\n"
    "1,1,32,54321,0,0.31\n"
  );
  std::string binPath = csv.substr(0, csv.rfind('.')) + "_grids.bin";
  {
    int W = 52, H = 30;
    int frameBytes = 4 + W * H * 5; // header + types(1) + mvx(2) + mvy(2)
    uint32_t gs = (uint32_t)frameBytes;
    std::vector<uint8_t> frameBuf(frameBytes);
    memcpy(frameBuf.data(), &gs, 4);
    memset(frameBuf.data() + 4, 0, frameBytes - 4);
    std::ofstream of(binPath, std::ios::binary);
    of.write((const char*)frameBuf.data(), frameBuf.size()); // frame 0
    of.write((const char*)frameBuf.data(), frameBuf.size()); // frame 1
    of.close();
  }
  HWPreAnalyzer ana;
  CHECK_EQ(ana.init(csv), 0, "init");
  const HWFrameMetadata* pMeta = nullptr;
  int ret = ana.getFrameMetadata(0, pMeta);
  CHECK_EQ(ret, 0, "get POC 0 OK");
  CHECK_TRUE(pMeta != nullptr, "meta not null");
  CHECK_EQ(pMeta->m_iPOC, 0, "POC 0");
  CHECK_EQ(pMeta->m_iQP, 28, "QP 28");

  pMeta = nullptr;
  ret = ana.getFrameMetadata(999, pMeta);
  CHECK_EQ(ret, 1, "invalid POC -> 1");

  ana.uninit();
  std::remove(csv.c_str());
  std::remove(binPath.c_str());
  return 0;
}

static int testGetComplexity()
{
  std::string csv = createTempCSV(
    "1,832,480\n"
    "poc,frameType,qp,bits,sceneCut,mvComplexity\n"
    "0,0,28,200000,1,0.75\n"
    "1,1,32,50000,0,0.31\n"
  );
  std::string binPath = csv.substr(0, csv.rfind('.')) + "_grids.bin";
  {
    int W = 52, H = 30;
    int frameBytes = 4 + W * H * 5;
    uint32_t gs = (uint32_t)frameBytes;
    std::vector<uint8_t> frameBuf(frameBytes);
    memcpy(frameBuf.data(), &gs, 4);
    memset(frameBuf.data() + 4, 0, frameBytes - 4);
    std::ofstream of(binPath, std::ios::binary);
    of.write((const char*)frameBuf.data(), frameBuf.size());
    of.write((const char*)frameBuf.data(), frameBuf.size());
    of.close();
  }
  HWPreAnalyzer ana;
  CHECK_EQ(ana.init(csv), 0, "init");
  float c0 = 0, c1 = 0;
  int ret = ana.getFrameComplexity(0, c0);
  CHECK_EQ(ret, 0, "complexity POC 0");
  CHECK_TRUE(c0 > 1.0f, "high bit frame > 1.0");

  ret = ana.getFrameComplexity(1, c1);
  CHECK_EQ(ret, 0, "complexity POC 1");
  CHECK_TRUE(c1 < 1.0f, "low bit frame < 1.0");

  ana.uninit();
  std::remove(csv.c_str());
  std::remove(binPath.c_str());
  return 0;
}

static int testGetSceneCut()
{
  std::string csv = createTempCSV(
    "1,832,480\n"
    "poc,frameType,qp,bits,sceneCut,mvComplexity\n"
    "0,0,28,123456,1,0.75\n"
    "1,1,32,54321,0,0.31\n"
  );
  std::string binPath = csv.substr(0, csv.rfind('.')) + "_grids.bin";
  {
    int W = 52, H = 30;
    int frameBytes = 4 + W * H * 5;
    uint32_t gs = (uint32_t)frameBytes;
    std::vector<uint8_t> frameBuf(frameBytes);
    memcpy(frameBuf.data(), &gs, 4);
    memset(frameBuf.data() + 4, 0, frameBytes - 4);
    std::ofstream of(binPath, std::ios::binary);
    of.write((const char*)frameBuf.data(), frameBuf.size());
    of.write((const char*)frameBuf.data(), frameBuf.size());
    of.close();
  }
  HWPreAnalyzer ana;
  CHECK_EQ(ana.init(csv), 0, "init");
  bool sc = false;
  int ret = ana.getSceneCut(0, sc);
  CHECK_EQ(ret, 0, "scene cut POC 0");
  CHECK_TRUE(sc, "POC 0 is scene cut");

  sc = false;
  ret = ana.getSceneCut(1, sc);
  CHECK_EQ(ret, 0, "scene cut POC 1");
  CHECK_FALSE(sc, "POC 1 not scene cut");

  ana.uninit();
  std::remove(csv.c_str());
  std::remove(binPath.c_str());
  return 0;
}

static int testGetMVField()
{
  std::string csv = createTempCSV(
    "1,832,480\n"
    "poc,frameType,qp,bits,sceneCut,mvComplexity\n"
    "0,0,28,123456,1,0.75\n"
  );
  std::string binPath = csv.substr(0, csv.rfind('.')) + "_grids.bin";
  {
    int W = 52, H = 30;
    uint32_t gs = (uint32_t)(4 + W * H * 5);
    std::ofstream of(binPath, std::ios::binary);
    of.write((const char*)&gs, 4);
    std::vector<uint8_t> types(W * H, 0);
    std::vector<int16_t> mv(W * H * 2, 0);
    mv[0] = 7; mv[1] = -3;
    of.write((const char*)types.data(), types.size());
    of.write((const char*)mv.data(), mv.size() * 2);
  }
  HWPreAnalyzer ana;
  CHECK_EQ(ana.init(csv), 0, "init");
  const HWMV* pGrid = nullptr;
  int gw = 0, gh = 0;
  int ret = ana.getMVField(0, pGrid, gw, gh);
  CHECK_EQ(ret, 0, "get MV field OK");
  CHECK_EQ(gw, 52, "grid w");
  CHECK_EQ(gh, 30, "grid h");
  CHECK_EQ(pGrid[0].x, 7, "first MV x");
  CHECK_EQ(pGrid[0].y, -3, "first MV y");

  ret = ana.getMVField(999, pGrid, gw, gh);
  CHECK_EQ(ret, 1, "invalid POC -> 1");

  ana.uninit();
  std::remove(csv.c_str());
  std::remove(binPath.c_str());
  return 0;
}

static int testLifecycleUninitTwice()
{
  HWPreAnalyzer ana;
  CHECK_EQ(ana.uninit(), 0, "first uninit OK");
  CHECK_EQ(ana.uninit(), 0, "second uninit OK");
  return 0;
}

// ── Test dispatch ────────────────────────────────────────────────

struct TestEntry
{
  int   id;
  int (*fn)();
  const char* name;
};

static const TestEntry g_tests[] = {
  { HW_BITSTREAM_LOAD_FRONTMATTER,    testBitstreamLoadFrontmatter,    "HW_BITSTREAM_LOAD_FRONTMATTER" },
  { HW_BITSTREAM_MISSING_FILE,        testBitstreamMissingFile,        "HW_BITSTREAM_MISSING_FILE" },
  { HW_BITSTREAM_CORRUPT_CSV,         testBitstreamCorruptCSV,         "HW_BITSTREAM_CORRUPT_CSV" },
  { HW_BITSTREAM_ZERO_FRAMES,         testBitstreamZeroFrames,         "HW_BITSTREAM_ZERO_FRAMES" },
  { HW_BITSTREAM_RELEASE,             testBitstreamRelease,            "HW_BITSTREAM_RELEASE" },
  { HW_BITSTREAM_PARSE_GRID,          testBitstreamParseGrid,          "HW_BITSTREAM_PARSE_GRID" },
  { HW_BITSTREAM_PARSE_TRUNCATED,     testBitstreamParseTruncated,     "HW_BITSTREAM_PARSE_TRUNCATED" },

  { HW_MV_VARIANCE_ZERO,              testMVVarianceZero,              "HW_MV_VARIANCE_ZERO" },
  { HW_MV_VARIANCE_HIGH,              testMVVarianceHigh,              "HW_MV_VARIANCE_HIGH" },
  { HW_MV_VARIANCE_SINGLE,            testMVVarianceSingle,            "HW_MV_VARIANCE_SINGLE" },

  { HW_PARTITION_ENTROPY_UNIFORM,     testEntropyUniform,              "HW_PARTITION_ENTROPY_UNIFORM" },
  { HW_PARTITION_ENTROPY_MIXED,       testEntropyMixed,                "HW_PARTITION_ENTROPY_MIXED" },
  { HW_PARTITION_ENTROPY_TWO,         testEntropyTwo,                  "HW_PARTITION_ENTROPY_TWO" },

  { HW_MOTION_BOUNDARY_8PEL,          testBoundary8pel,                "HW_MOTION_BOUNDARY_8PEL" },
  { HW_MOTION_BOUNDARY_4PEL,          testBoundary4pel,                "HW_MOTION_BOUNDARY_4PEL" },
  { HW_MOTION_BOUNDARY_1PEL,          testBoundary1pel,                "HW_MOTION_BOUNDARY_1PEL" },
  { HW_MOTION_BOUNDARY_IDENTICAL,     testBoundaryIdentical,           "HW_MOTION_BOUNDARY_IDENTICAL" },

  { HW_CU_HINT_HOMOGENEOUS_NO_SPLIT,  testHintHomogeneousNoSplit,      "HW_CU_HINT_HOMOGENEOUS_NO_SPLIT" },
  { HW_CU_HINT_BOUNDARY_HORIZ,        testHintBoundaryHoriz,           "HW_CU_HINT_BOUNDARY_HORIZ" },
  { HW_CU_HINT_BOUNDARY_VERT,         testHintBoundaryVert,            "HW_CU_HINT_BOUNDARY_VERT" },
  { HW_CU_HINT_BOUNDARY_BOTH,         testHintBoundaryBoth,            "HW_CU_HINT_BOUNDARY_BOTH" },
  { HW_CU_HINT_MEDIUM_VARIANCE,       testHintMediumVariance,          "HW_CU_HINT_MEDIUM_VARIANCE" },
  { HW_CU_HINT_HIGH_VARIANCE,         testHintHighVariance,            "HW_CU_HINT_HIGH_VARIANCE" },
  { HW_CU_HINT_HIGH_ENTROPY,          testHintHighEntropy,             "HW_CU_HINT_HIGH_ENTROPY" },
  { HW_CU_HINT_LOW_CONFIDENCE,        testHintLowConfidence,           "HW_CU_HINT_LOW_CONFIDENCE" },
  { HW_CU_HINT_INSUFFICIENT_DATA,     testHintInsufficientData,        "HW_CU_HINT_INSUFFICIENT_DATA" },
  { HW_CU_HINT_FRAME_BOUNDARY,        testHintFrameBoundary,           "HW_CU_HINT_FRAME_BOUNDARY" },

  { HW_EXTRACT_SUBGRID_FULL,          testExtractSubgridFull,          "HW_EXTRACT_SUBGRID_FULL" },
  { HW_EXTRACT_SUBGRID_TRUNCATED,     testExtractSubgridTruncated,     "HW_EXTRACT_SUBGRID_TRUNCATED" },
  { HW_EXTRACT_SUBGRID_OUT_OF_BOUNDS, testExtractSubgridOutOfBounds,   "HW_EXTRACT_SUBGRID_OUT_OF_BOUNDS" },

  { HW_DETERMINE_SPLIT_HORIZ,         testSplitHoriz,                  "HW_DETERMINE_SPLIT_HORIZ" },
  { HW_DETERMINE_SPLIT_VERT,          testSplitVert,                   "HW_DETERMINE_SPLIT_VERT" },
  { HW_DETERMINE_SPLIT_BOTH,          testSplitBoth,                   "HW_DETERMINE_SPLIT_BOTH" },
  { HW_DETERMINE_SPLIT_NEITHER,       testSplitNeither,                "HW_DETERMINE_SPLIT_NEITHER" },

  { HW_INIT_LOAD,                     testInitLoad,                    "HW_INIT_LOAD" },
  { HW_INIT_MISSING_FILE,             testInitMissingFile,             "HW_INIT_MISSING_FILE" },
  { HW_INIT_MISSING_BIN,              testInitMissingBin,              "HW_INIT_MISSING_BIN" },
  { HW_GET_FRAME_META,                testGetFrameMeta,                "HW_GET_FRAME_META" },
  { HW_GET_COMPLEXITY,                testGetComplexity,               "HW_GET_COMPLEXITY" },
  { HW_GET_SCENE_CUT,                 testGetSceneCut,                 "HW_GET_SCENE_CUT" },
  { HW_GET_MV_FIELD,                  testGetMVField,                  "HW_GET_MV_FIELD" },
  { HW_LIFECYCLE_UNINIT_TWICE,        testLifecycleUninitTwice,        "HW_LIFECYCLE_UNINIT_TWICE" },
};

static const int g_numTests_total = sizeof(g_tests) / sizeof(g_tests[0]);

int main(int argc, char* argv[])
{
  int testId = 0;
  if (argc > 1)
    testId = std::atoi(argv[1]);

  if (testId == 0)
  {
    // Run all tests
    for (int i = 0; i < g_numTests_total; i++)
      TEST(g_tests[i].fn());
  }
  else
  {
    // Run single test
    for (int i = 0; i < g_numTests_total; i++)
    {
      if (g_tests[i].id == testId)
      {
        TEST(g_tests[i].fn());
        break;
      }
    }
  }

  std::cout << g_numTests << " tests, "
            << g_numFails << " failures"
            << std::endl;

  return g_numFails > 0 ? 1 : 0;
}
