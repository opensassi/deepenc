#include "HWPreAnalyzer.h"
#include "HWBitstreamParser.h"
#include "HWCuPartitionAnalyzer.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace vvenc;

// ── Test macros ───────────────────────────────────────────────────
static int g_numTests = 0;
static int g_numFails = 0;

#define TEST(x)   { int res = (x); g_numTests++; g_numFails += (res != 0); }
#define TESTT(x,w){ int res = (x); g_numTests++; g_numFails += (res != 0); }
#define ERROR(w)  { g_numTests++; g_numFails++; }

#define CHECK_EQ(a, b, msg) \
  do { \
    if ((a) != (b)) { \
      std::cerr << "FAIL [" << msg << "]: expected " << (b) << " got " << (a) << std::endl; \
      return 1; \
    } \
  } while(0)

#define CHECK_NEAR(a, b, eps, msg) \
  do { \
    float diff = std::fabs((float)(a) - (float)(b)); \
    if (diff > (eps)) { \
      std::cerr << "FAIL [" << msg << "]: expected ~" << (b) << " got " << (a) << " (diff=" << diff << ")" << std::endl; \
      return 1; \
    } \
  } while(0)

#define CHECK_TRUE(cond, msg) \
  do { \
    if (!(cond)) { \
      std::cerr << "FAIL [" << msg << "]: expected true" << std::endl; \
      return 1; \
    } \
  } while(0)

#define CHECK_FALSE(cond, msg) \
  do { \
    if ((cond)) { \
      std::cerr << "FAIL [" << msg << "]: expected false" << std::endl; \
      return 1; \
    } \
  } while(0)

// ===================================================================
// Test helpers
// ===================================================================

static MBPartitionGrid makeUniformGrid(int w, int h, uint8_t mbType,
                                       int16_t mvX, int16_t mvY)
{
  MBPartitionGrid grid;
  grid.m_iWidth  = w;
  grid.m_iHeight = h;
  for (int y = 0; y < h; y++)
  {
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
  }
  return grid;
}

static MBPartitionGrid makeMixedGrid(int w, int h,
                                     uint8_t typeLeft, uint8_t typeRight,
                                     int16_t mvLeftX, int16_t mvLeftY,
                                     int16_t mvRightX, int16_t mvRightY)
{
  MBPartitionGrid grid;
  grid.m_iWidth  = w;
  grid.m_iHeight = h;
  for (int y = 0; y < h; y++)
  {
    for (int x = 0; x < w; x++)
    {
      MBPartitionInfo info;
      info.m_iPosX = x;
      info.m_iPosY = y;
      if (x < w / 2)
      {
        info.m_uiMBType  = typeLeft;
        info.m_cMV.x     = mvLeftX;
        info.m_cMV.y     = mvLeftY;
      }
      else
      {
        info.m_uiMBType  = typeRight;
        info.m_cMV.x     = mvRightX;
        info.m_cMV.y     = mvRightY;
      }
      info.m_uiSubMBMask = info.m_uiMBType;
      grid.m_cMBs.push_back(info);
    }
  }
  return grid;
}

// ===================================================================
// Test: MV Variance
// ===================================================================
static int testMVVarianceZero()
{
  HWCuPartitionAnalyzer ana;
  HWMV mvs[16];
  for (int i = 0; i < 16; i++)
    mvs[i] = {5, -3};
  float v = ana.computeMVVariance(mvs, 16);
  CHECK_NEAR(v, 0.0f, 0.0001f, "MV variance zero");
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
  CHECK_TRUE(v > 0.5f, "MV variance high");
  return 0;
}

static int testMVVarianceSingle()
{
  HWCuPartitionAnalyzer ana;
  HWMV mvs[1] = {{42, -17}};
  float v = ana.computeMVVariance(mvs, 1);
  CHECK_NEAR(v, 0.0f, 0.0001f, "MV variance single");
  return 0;
}

// ===================================================================
// Test: Partition Entropy
// ===================================================================
static int testEntropyUniform()
{
  HWCuPartitionAnalyzer ana;
  uint8_t types[16];
  memset(types, 0, 16);
  float e = ana.computePartitionEntropy(types, 16);
  CHECK_NEAR(e, 0.0f, 0.0001f, "Entropy uniform");
  return 0;
}

static int testEntropyMixed()
{
  HWCuPartitionAnalyzer ana;
  uint8_t types[8];
  for (int i = 0; i < 8; i++)
    types[i] = (uint8_t)i;
  float e = ana.computePartitionEntropy(types, 8);
  CHECK_NEAR(e, 1.0f, 0.001f, "Entropy mixed");
  return 0;
}

static int testEntropyTwo()
{
  HWCuPartitionAnalyzer ana;
  uint8_t types[8];
  for (int i = 0; i < 4; i++) types[i] = 0;
  for (int i = 4; i < 8; i++) types[i] = 1;
  float e = ana.computePartitionEntropy(types, 8);
  CHECK_NEAR(e, 1.0f, 0.001f, "Entropy two types");
  return 0;
}

// ===================================================================
// Test: Motion Boundary
// ===================================================================
static int testBoundary8pel()
{
  HWCuPartitionAnalyzer ana;
  HWMV mvs[4] = {{0,0}, {0,0}, {8,0}, {8,0}};
  bool h = false, v = false;
  bool found = ana.hasMotionBoundary(mvs, 2, 2, h, v);
  CHECK_TRUE(found, "Boundary 8pel exists");
  return 0;
}

static int testBoundary4pel()
{
  HWCuPartitionAnalyzer ana;
  HWMV mvs[4] = {{0,0}, {0,0}, {4,0}, {4,0}};
  bool h = false, v = false;
  bool found = ana.hasMotionBoundary(mvs, 2, 2, h, v);
  CHECK_TRUE(found, "Boundary 4pel exists (inclusive)");
  return 0;
}

static int testBoundary1pel()
{
  HWCuPartitionAnalyzer ana;
  HWMV mvs[4] = {{0,0}, {0,0}, {1,0}, {1,0}};
  bool h = false, v = false;
  bool found = ana.hasMotionBoundary(mvs, 2, 2, h, v);
  CHECK_FALSE(found, "Boundary 1pel absent");
  return 0;
}

static int testBoundaryIdentical()
{
  HWCuPartitionAnalyzer ana;
  HWMV mvs[4] = {{3,3}, {3,3}, {3,3}, {3,3}};
  bool h = false, v = false;
  bool found = ana.hasMotionBoundary(mvs, 2, 2, h, v);
  CHECK_FALSE(found, "Boundary identical absent");
  return 0;
}

// ===================================================================
// Test: CU Split Hint
// ===================================================================
static int testHintNoSplit()
{
  HWCuPartitionAnalyzer ana;
  MBPartitionGrid grid = makeUniformGrid(8, 8, 0, 0, 0);
  CUSplitHint hint;
  int ret = ana.computeHint(grid, 0, 0, 8, hint);
  CHECK_EQ(ret, 0, "noSplit return");
  CHECK_TRUE(hint.m_bNoSplit, "noSplit flag");
  CHECK_FALSE(hint.m_bForceSplit, "noSplit not force");
  CHECK_TRUE(hint.m_fConfidence > 0.8f, "noSplit confidence > 0.8");
  return 0;
}

static int testHintForceSplitBoundary()
{
  HWCuPartitionAnalyzer ana;
  MBPartitionGrid grid = makeMixedGrid(8, 8, 0, 1, 0, 0, 10, 0);
  CUSplitHint hint;
  int ret = ana.computeHint(grid, 0, 0, 8, hint);
  CHECK_EQ(ret, 0, "forceSplit return");
  CHECK_TRUE(hint.m_bForceSplit, "forceSplit flag");
  CHECK_TRUE(hint.m_fConfidence > 0.7f, "forceSplit confidence > 0.7");
  return 0;
}

static int testHintMediumVariance()
{
  // Identical MVs (no boundary), entropy from 3 type-1 cells among 61 type-0: ~0.16
  // Falls through to branch 3 (medium) -> conf=0.6
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
      info.m_cMV.x = 0; info.m_cMV.y = 0;
      grid.m_cMBs.push_back(info);
    }
  CUSplitHint hint;
  int ret = ana.computeHint(grid, 0, 0, 8, hint);
  CHECK_EQ(ret, 0, "medium return");
  CHECK_TRUE(hint.m_bNoSplit, "medium noSplit");
  CHECK_NEAR(hint.m_fConfidence, 0.6f, 0.05f, "medium confidence ~0.6");
  return 0;
}

static int testHintHighVariance()
{
  // mv.x alternates by column (x%2), mv.y alternates by row (y%2)
  // -> both boundaries detected -> QT + conf=0.8
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
  CHECK_EQ(ret, 0, "high variance return");
  CHECK_TRUE(hint.m_bForceSplit, "high variance forceSplit");
  CHECK_EQ(hint.m_eSplitType, CU_SPLIT_QT, "high variance split QT");
  CHECK_NEAR(hint.m_fConfidence, 0.92f, 0.01f, "high variance conf 0.92");
  return 0;
}

static int testHintInsufficient()
{
  HWCuPartitionAnalyzer ana;
  MBPartitionGrid grid = makeUniformGrid(4, 4, 0, 0, 0);
  CUSplitHint hint;
  int ret = ana.computeHint(grid, 2, 2, 8, hint);
  CHECK_EQ(ret, 1, "insufficient return");
  return 0;
}

static int testHintLowConfidence()
{
  // Identical MVs (no boundary), 9 type-1 among 64: entropy=0.586, betw. MED(0.3) and HIGH(0.6) -> else branch -> conf=0.3
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
      info.m_cMV.x = 0; info.m_cMV.y = 0;
      grid.m_cMBs.push_back(info);
    }
  CUSplitHint hint;
  int ret = ana.computeHint(grid, 0, 0, 8, hint);
  CHECK_EQ(ret, 0, "low conf return");
  CHECK_NEAR(hint.m_fConfidence, 0.3f, 0.02f, "low confidence ~0.3");
  return 0;
}

// ===================================================================
// Test: Sub-grid extraction
// ===================================================================
static int testExtractFull()
{
  HWCuPartitionAnalyzer ana;
  MBPartitionGrid grid = makeUniformGrid(16, 16, 42, 7, -3);
  uint8_t types[64];
  HWMV mvs[64];
  int aw = 0, ah = 0;
  int ret = ana.extractSubGrid(grid, 4, 4, 8, types, mvs, aw, ah);
  CHECK_EQ(ret, 0, "extract full return");
  CHECK_EQ(aw, 8, "extract full w");
  CHECK_EQ(ah, 8, "extract full h");
  CHECK_EQ(types[0], 42, "extract full type");
  CHECK_EQ(mvs[0].x, 7, "extract full mv x");
  CHECK_EQ(mvs[0].y, -3, "extract full mv y");
  return 0;
}

static int testExtractTruncated()
{
  HWCuPartitionAnalyzer ana;
  MBPartitionGrid grid = makeUniformGrid(10, 16, 0, 0, 0);
  uint8_t types[64];
  HWMV mvs[64];
  int aw = 0, ah = 0;
  int ret = ana.extractSubGrid(grid, 8, 0, 8, types, mvs, aw, ah);
  CHECK_EQ(ret, 0, "extract truncated return");
  CHECK_EQ(aw, 2, "extract truncated w");
  CHECK_EQ(ah, 8, "extract truncated h");
  return 0;
}

static int testExtractOutOfBounds()
{
  HWCuPartitionAnalyzer ana;
  MBPartitionGrid grid = makeUniformGrid(8, 8, 0, 0, 0);
  uint8_t types[4];
  HWMV mvs[4];
  int aw = 0, ah = 0;
  int ret = ana.extractSubGrid(grid, 20, 20, 2, types, mvs, aw, ah);
  CHECK_EQ(ret, 1, "extract oob return");
  return 0;
}

// ===================================================================
// Test: Split type determination
// ===================================================================
static int testSplitHoriz()
{
  HWCuPartitionAnalyzer ana;
  CHECK_EQ(ana.determineSplitType(true, false), CU_SPLIT_BT_H, "split horiz");
  return 0;
}

static int testSplitVert()
{
  HWCuPartitionAnalyzer ana;
  CHECK_EQ(ana.determineSplitType(false, true), CU_SPLIT_BT_V, "split vert");
  return 0;
}

static int testSplitBoth()
{
  HWCuPartitionAnalyzer ana;
  CHECK_EQ(ana.determineSplitType(true, true), CU_SPLIT_QT, "split both");
  return 0;
}

static int testSplitNeither()
{
  HWCuPartitionAnalyzer ana;
  CHECK_EQ(ana.determineSplitType(false, false), CU_SPLIT_NONE, "split neither");
  return 0;
}

// ===================================================================
// Test: Facade lifecycle
// ===================================================================
static int testLifecycleInit()
{
  HWPreAnalyzer ana;
  int ret = ana.init("/nonexistent/path.csv");
  CHECK_EQ(ret, -1, "init missing file");
  return 0;
}

static int testLifecycleIsInit()
{
  HWPreAnalyzer ana;
  CHECK_FALSE(ana.isInitialized(), "not init before init");
  return 0;
}

static int testLifecycleUninitTwice()
{
  HWPreAnalyzer ana;
  CHECK_EQ(ana.uninit(), 0, "uninit first");
  CHECK_EQ(ana.uninit(), 0, "uninit second");
  return 0;
}

// ===================================================================
// Test: Bitstream parser
// ===================================================================
static int testParserLoadMissing()
{
  HWBitstreamParser parser;
  std::vector<HWFrameMetadata> frames;
  int w = 0, h = 0, avg = 0;
  int ret = parser.loadFrontmatter("/nonexistent.csv", frames, w, h, avg);
  CHECK_EQ(ret, -1, "parser missing file");
  return 0;
}

static int testParserRelease()
{
  HWBitstreamParser parser;
  CHECK_EQ(parser.release(), 0, "parser release");
  CHECK_FALSE(parser.isValidFrameIndex(0), "invalid after release");
  return 0;
}

// ===================================================================
// Benchmark helpers
// ===================================================================
static double benchMVVariance(int iterations)
{
  HWCuPartitionAnalyzer ana;
  HWMV mvs[4096];
  for (int i = 0; i < 4096; i++)
  {
    mvs[i].x = (int16_t)(rand() % 200 - 100);
    mvs[i].y = (int16_t)(rand() % 200 - 100);
  }
  auto start = std::chrono::high_resolution_clock::now();
  volatile float result = 0.0f;
  for (int i = 0; i < iterations; i++)
    result += ana.computeMVVariance(mvs, 4096);
  auto end = std::chrono::high_resolution_clock::now();
  double ms = std::chrono::duration<double, std::milli>(end - start).count();
  return ms;
}

static double benchEntropy(int iterations)
{
  HWCuPartitionAnalyzer ana;
  uint8_t types[4096];
  for (int i = 0; i < 4096; i++)
    types[i] = (uint8_t)(rand() % 16);
  auto start = std::chrono::high_resolution_clock::now();
  volatile float result = 0.0f;
  for (int i = 0; i < iterations; i++)
    result += ana.computePartitionEntropy(types, 4096);
  auto end = std::chrono::high_resolution_clock::now();
  double ms = std::chrono::duration<double, std::milli>(end - start).count();
  return ms;
}

static double benchComputeHint(int iterations)
{
  HWCuPartitionAnalyzer ana;
  MBPartitionGrid grid = makeUniformGrid(8, 8, 0, 0, 0);
  CUSplitHint hint;
  auto start = std::chrono::high_resolution_clock::now();
  volatile int ret = 0;
  for (int i = 0; i < iterations; i++)
    ret += ana.computeHint(grid, 0, 0, 8, hint);
  auto end = std::chrono::high_resolution_clock::now();
  double ms = std::chrono::duration<double, std::milli>(end - start).count();
  return ms;
}

// ===================================================================
// CSV/Grid file generation
// ===================================================================
static int genCSV(const std::string& outputDir, int gridW, int gridH,
                  int numFrames, const std::string& pattern)
{
  std::string csvPath = outputDir + "/hw_metadata.csv";
  std::string binPath = outputDir + "/hw_metadata_grids.bin";

  std::ofstream csv(csvPath);
  if (!csv.is_open())
  {
    std::cerr << "Cannot write " << csvPath << std::endl;
    return 1;
  }

  csv << "1," << (gridW * 16) << "," << (gridH * 16) << "\n";
  csv << "poc,frameType,qp,bits,sceneCut,mvComplexity\n";

  std::ofstream bin(binPath, std::ios::binary);
  if (!bin.is_open())
  {
    std::cerr << "Cannot write " << binPath << std::endl;
    return 1;
  }

  int gridBytes = 4 + gridW * gridH * 5; // header + types + mvs

  srand(42);

  for (int f = 0; f < numFrames; f++)
  {
    int poc    = f;
    int type   = (f == 0) ? 0 : ((f % 3 == 0) ? 2 : 1);
    int qp     = 28 + (rand() % 8);
    int bits   = 50000 + (rand() % 200000);
    int scene  = (f == 0 || f == numFrames / 2) ? 1 : 0;
    float mvC  = (float)(rand() % 100) / 100.0f;

    csv << poc << "," << type << "," << qp << "," << bits
        << "," << scene << "," << mvC << "\n";

    uint32_t gridSize = (uint32_t)gridBytes;
    bin.write(reinterpret_cast<const char*>(&gridSize), 4);

    std::vector<uint8_t> mbTypes(gridW * gridH);
    std::vector<int16_t> mvX(gridW * gridH);
    std::vector<int16_t> mvY(gridW * gridH);

    for (int y = 0; y < gridH; y++)
    {
      for (int x = 0; x < gridW; x++)
      {
        int idx = y * gridW + x;
        if (pattern == "uniform")
        {
          mbTypes[idx] = 0;
          mvX[idx] = 0;
          mvY[idx] = 0;
        }
        else if (pattern == "boundary")
        {
          if (x < gridW / 2)
          {
            mbTypes[idx] = 0;
            mvX[idx] = 0;
            mvY[idx] = 0;
          }
          else
          {
            mbTypes[idx] = 1;
            mvX[idx] = 10;
            mvY[idx] = 0;
          }
        }
        else if (pattern == "mixed")
        {
          mbTypes[idx] = (uint8_t)(((x + y) % 4));
          mvX[idx] = (int16_t)((x - gridW / 2) * 2);
          mvY[idx] = (int16_t)((y - gridH / 2) * 2);
        }
        else
        {
          mbTypes[idx] = (uint8_t)(rand() % 8);
          mvX[idx] = (int16_t)(rand() % 200 - 100);
          mvY[idx] = (int16_t)(rand() % 200 - 100);
        }
      }
    }

    bin.write(reinterpret_cast<const char*>(mbTypes.data()), gridW * gridH);
    // Interleaved mvX, mvY pairs
    for (int i = 0; i < gridW * gridH; i++)
    {
      int16_t pair[2] = { mvX[i], mvY[i] };
      bin.write(reinterpret_cast<const char*>(pair), sizeof(pair));
    }
  }

  csv.close();
  bin.close();

  std::cout << "Generated " << csvPath << " (" << numFrames << " frames)"
            << "\n         " << binPath << " (" << gridBytes * numFrames << " bytes)"
            << std::endl;
  return 0;
}

// ===================================================================
// Validate command
// ===================================================================
static int validate(const std::string& csvPath, const std::string& binPath)
{
  // init() derives bin path from csv path by replacing .csv with _grids.bin
  // Copy the provided bin to that expected location
  std::string expectedBin = csvPath.substr(0, csvPath.rfind('.')) + "_grids.bin";
  // Copy user-provided bin to expected location if different
  if (binPath != expectedBin)
  {
    std::ifstream src(binPath, std::ios::binary);
    std::ofstream dst(expectedBin, std::ios::binary);
    if (src.is_open() && dst.is_open())
      dst << src.rdbuf();
  }

  HWPreAnalyzer ana;
  int ret = ana.init(csvPath);
  if (ret != 0)
  {
    std::cerr << "init() returned " << ret << std::endl;
    return 1;
  }

  std::cout << "HWPreAnalyzer initialized OK"
            << "\n  frames: " << ana.getNumFrames()
            << std::endl;

  const HWFrameMetadata* pMeta = nullptr;
  ret = ana.getFrameMetadata(0, pMeta);
  if (ret != 0 || !pMeta)
  {
    std::cerr << "getFrameMetadata(0) failed" << std::endl;
    return 1;
  }

  std::cout << "Frame 0: POC=" << pMeta->m_iPOC
            << " QP=" << pMeta->m_iQP
            << " bits=" << pMeta->m_uBits
            << " sceneCut=" << pMeta->m_bSceneCut
            << " grid=" << pMeta->m_cMBGrid.m_iWidth
            << "x" << pMeta->m_cMBGrid.m_iHeight
            << std::endl;

  float complexity = 0.0f;
  ret = ana.getFrameComplexity(0, complexity);
  if (ret == 0)
    std::cout << "Frame 0 complexity: " << complexity << std::endl;

  CUSplitHint hint;
  ret = ana.getCUSplitHint(0, 0, 8, hint);
  if (ret == 0)
    std::cout << "CU hint: forceSplit=" << hint.m_bForceSplit
              << " noSplit=" << hint.m_bNoSplit
              << " confidence=" << hint.m_fConfidence
              << std::endl;

  const HWMV* mvGrid = nullptr;
  int gridW = 0, gridH = 0;
  ret = ana.getMVField(0, mvGrid, gridW, gridH);
  if (ret == 0)
    std::cout << "MV field: " << gridW << "x" << gridH
              << " first MV=(" << mvGrid[0].x << "," << mvGrid[0].y << ")"
              << std::endl;

  ana.uninit();
  std::cout << "Validation OK" << std::endl;
  return 0;
}

// ===================================================================
// Main
// ===================================================================
int main(int argc, char* argv[])
{
  if (argc < 2)
  {
    std::cout << "Usage: hw_bench <command> [options]\n\n"
              << "Commands:\n"
              << "  test                    Run all self-tests\n"
              << "  bench [--iterations N]  Run microbenchmarks\n"
              << "  gen-csv                 Generate test CSV+bin fixtures\n"
              << "    --grid-w W     MB grid width  (default: 52)\n"
              << "    --grid-h H     MB grid height (default: 30)\n"
              << "    --num-frames N Frame count    (default: 16)\n"
              << "    --pattern P    uniform|boundary|mixed|random (default: mixed)\n"
              << "    --output DIR   Output directory (default: /tmp/hw_test)\n"
              << "  validate --csv PATH --bin PATH\n"
              << std::endl;
    return 0;
  }

  std::string cmd = argv[1];

  if (cmd == "test")
  {
    int total = 0, failed = 0;

    auto run = [&](const char* name, int (*fn)()) {
      int ret = fn();
      total++;
      if (ret != 0) failed++;
      std::cout << (ret == 0 ? "PASS" : "FAIL") << " " << name << std::endl;
    };

    // MV Variance
    run("MV variance zero",           testMVVarianceZero);
    run("MV variance high",           testMVVarianceHigh);
    run("MV variance single",         testMVVarianceSingle);
    // Partition Entropy
    run("Entropy uniform",            testEntropyUniform);
    run("Entropy mixed",              testEntropyMixed);
    run("Entropy two types",          testEntropyTwo);
    // Motion Boundary
    run("Boundary 8pel",              testBoundary8pel);
    run("Boundary 4pel (inclusive)",  testBoundary4pel);
    run("Boundary 1pel absent",       testBoundary1pel);
    run("Boundary identical absent",  testBoundaryIdentical);
    // CU Split Hint
    run("Hint noSplit high conf",     testHintNoSplit);
    run("Hint forceSplit boundary",   testHintForceSplitBoundary);
    run("Hint medium variance",       testHintMediumVariance);
    run("Hint high variance",         testHintHighVariance);
    run("Hint insufficient data",     testHintInsufficient);
    run("Hint low confidence",        testHintLowConfidence);
    // Sub-grid
    run("Extract full",               testExtractFull);
    run("Extract truncated",          testExtractTruncated);
    run("Extract out of bounds",      testExtractOutOfBounds);
    // Split type
    run("Split horizontal",           testSplitHoriz);
    run("Split vertical",             testSplitVert);
    run("Split both",                 testSplitBoth);
    run("Split neither",              testSplitNeither);
    // Facade lifecycle
    run("Lifecycle init missing",     testLifecycleInit);
    run("Lifecycle isInit false",     testLifecycleIsInit);
    run("Lifecycle uninit twice",     testLifecycleUninitTwice);
    // Parser
    run("Parser load missing",        testParserLoadMissing);
    run("Parser release",             testParserRelease);

    std::cout << "\n" << total << " tests, " << failed << " failed" << std::endl;
    return failed ? 1 : 0;
  }

  if (cmd == "bench")
  {
    int iterations = 100000;
    for (int i = 2; i < argc - 1; i++)
    {
      if (std::string(argv[i]) == "--iterations")
        iterations = std::atoi(argv[i + 1]);
    }

    std::cout << "Running benchmarks (" << iterations << " iterations each)..." << std::endl;

    double t1 = benchMVVariance(iterations);
    std::cout << "  computeMVVariance (4096 MVs): "
              << (t1 / iterations * 1e6) << " ns/call"
              << "  total: " << t1 << " ms" << std::endl;

    double t2 = benchEntropy(iterations);
    std::cout << "  computePartitionEntropy (4096 types): "
              << (t2 / iterations * 1e6) << " ns/call"
              << "  total: " << t2 << " ms" << std::endl;

    double t3 = benchComputeHint(iterations);
    std::cout << "  computeHint (8x8 grid): "
              << (t3 / iterations * 1e6) << " ns/call"
              << "  total: " << t3 << " ms" << std::endl;

    return 0;
  }

  if (cmd == "gen-csv")
  {
    int gridW = 52, gridH = 30, numFrames = 16;
    std::string pattern = "mixed";
    std::string outputDir = "/tmp/hw_test";

    for (int i = 2; i < argc; i++)
    {
      if (i + 1 < argc)
      {
        if (std::string(argv[i]) == "--grid-w")     gridW     = std::atoi(argv[++i]);
        if (std::string(argv[i]) == "--grid-h")     gridH     = std::atoi(argv[++i]);
        if (std::string(argv[i]) == "--num-frames") numFrames = std::atoi(argv[++i]);
        if (std::string(argv[i]) == "--pattern")    pattern   = argv[++i];
        if (std::string(argv[i]) == "--output")     outputDir = argv[++i];
      }
    }

    return genCSV(outputDir, gridW, gridH, numFrames, pattern);
  }

  if (cmd == "validate")
  {
    std::string csvPath, binPath;
    for (int i = 2; i < argc - 1; i++)
    {
      if (std::string(argv[i]) == "--csv") csvPath = argv[++i];
      if (std::string(argv[i]) == "--bin") binPath = argv[++i];
    }
    if (csvPath.empty() || binPath.empty())
    {
      std::cerr << "Usage: hw_bench validate --csv <path> --bin <path>" << std::endl;
      return 1;
    }
    return validate(csvPath, binPath);
  }

  std::cerr << "Unknown command: " << cmd << std::endl;
  return 1;
}
