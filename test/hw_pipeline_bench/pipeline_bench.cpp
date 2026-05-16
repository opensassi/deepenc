#include "vvenc/vvenc.h"
#include "vvenc/vvencCfg.h"
#include "vvenc/version.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <sys/stat.h>

// ── Paths ─────────────────────────────────────────────────────────
static const char* g_inputYuv     = "perf/traces/hw_pipeline_baseline/input.yuv";
static const char* g_refPath      = "perf/traces/hw_pipeline_baseline/output.266";

// ── Logging callback ──────────────────────────────────────────────
static void msgCb(void*, int level, const char* fmt, va_list args)
{
  if (level <= 1) vfprintf(stderr, fmt, args);
}

// ── File I/O ──────────────────────────────────────────────────────
static int loadFile(const char* path, std::vector<uint8_t>& buf)
{
  FILE* f = fopen(path, "rb");
  if (!f) return -1;
  fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
  buf.resize(sz);
  if ((long)fread(buf.data(), 1, sz, f) != sz) { fclose(f); return -1; }
  fclose(f); return 0;
}

static int saveFile(const char* path, const uint8_t* data, size_t sz)
{
  FILE* f = fopen(path, "wb");
  if (!f) return -1;
  size_t written = fwrite(data, 1, sz, f);
  fclose(f);
  return (written == sz) ? 0 : -1;
}

// ── Fill YUV buffer ───────────────────────────────────────────────
static int fillYUVBuffer(vvencYUVBuffer* buf, const uint8_t* data,
                         int width, int height)
{
  for (int c = 0; c < 3; c++)
  {
    int row  = (c == 0) ? width : width / 2;
    int rows = (c == 0) ? height : height / 2;
    int stride = (int)buf->planes[c].stride;
    for (int y = 0; y < rows; y++)
      for (int x = 0; x < row; x++)
        buf->planes[c].ptr[y * stride + x] = data[y * row + x];
  }
  return 0;
}

// ── Generate HW metadata sidecar (CSV + binary) ──────────────────
static int genHWMetadata(int width, int height, int numFrames,
                         const char* csvPath, const char* binPath)
{
  int gridW = (width  + 15) / 16;
  int gridH = (height + 15) / 16;
  int frameBytes = 4 + gridW * gridH * 5;

  // CSV
  FILE* csv = fopen(csvPath, "w");
  if (!csv) return -1;
  fprintf(csv, "1,%d,%d\n", width, height);
  fprintf(csv, "poc,frameType,qp,bits,sceneCut,mvComplexity\n");
  for (int f = 0; f < numFrames; f++)
    fprintf(csv, "%d,%d,%d,%d,%d,%.2f\n",
            f, (f == 0) ? 0 : 1, 32, 100000 + f * 1000, (f == 0) ? 1 : 0, 0.5);
  fclose(csv);

  // Binary (uniform: all MBs type=0, mv=(0,0))
  FILE* bin = fopen(binPath, "wb");
  if (!bin) return -1;

  std::vector<uint8_t> frameBuf(frameBytes);
  uint32_t gs = (uint32_t)frameBytes;
  memcpy(frameBuf.data(), &gs, 4);
  memset(frameBuf.data() + 4, 0, frameBytes - 4);

  for (int f = 0; f < numFrames; f++)
    fwrite(frameBuf.data(), 1, frameBytes, bin);
  fclose(bin);
  return 0;
}

// ── Run encode ────────────────────────────────────────────────────
static int runEncode(vvencEncoder* enc, int width, int height,
                     const uint8_t* yuvData,
                     std::vector<uint8_t>& outputBits,
                     double& outMs)
{
  vvencYUVBuffer yuvBuf;
  vvenc_YUVBuffer_default(&yuvBuf);
  vvenc_YUVBuffer_alloc_buffer(&yuvBuf, VVENC_CHROMA_420, width, height);

  vvencAccessUnit au;
  vvenc_accessUnit_default(&au);
  vvenc_accessUnit_alloc_payload(&au, (size_t)width * height * 3 / 2);

  if (fillYUVBuffer(&yuvBuf, yuvData, width, height) != 0)
  {
    fprintf(stderr, "FATAL: fillYUVBuffer failed\n");
    vvenc_YUVBuffer_free_buffer(&yuvBuf);
    vvenc_accessUnit_free_payload(&au);
    return 1;
  }

  yuvBuf.sequenceNumber = 0;
  bool encodeDone = false;
  auto t0 = std::chrono::high_resolution_clock::now();

  int ret = vvenc_encode(enc, &yuvBuf, &au, &encodeDone);
  if (ret != 0)
  {
    fprintf(stderr, "FATAL: encode failed: %d %s\n", ret, vvenc_get_last_error(enc));
    vvenc_YUVBuffer_free_buffer(&yuvBuf);
    vvenc_accessUnit_free_payload(&au);
    return 1;
  }

  {
    auto collect = [&]()
    {
      if (au.payloadUsedSize > 0)
        outputBits.insert(outputBits.end(), au.payload, au.payload + au.payloadUsedSize);
    };
    collect();
    while (!encodeDone)
    {
      ret = vvenc_encode(enc, nullptr, &au, &encodeDone);
      if (ret != 0)
      {
        fprintf(stderr, "FATAL: flush failed: %d %s\n", ret, vvenc_get_last_error(enc));
        vvenc_YUVBuffer_free_buffer(&yuvBuf);
        vvenc_accessUnit_free_payload(&au);
        return 1;
      }
      collect();
    }
  }

  outMs = std::chrono::duration<double, std::milli>(
      std::chrono::high_resolution_clock::now() - t0).count();

  vvenc_YUVBuffer_free_buffer(&yuvBuf);
  vvenc_accessUnit_free_payload(&au);
  return 0;
}

// ── Configure encoder (returns cfg, caller owns) ──────────────────
static vvenc_config makeConfig(int width, int height, bool withHw,
                               const char* hwMetadataPath)
{
  vvenc_config cfg;
  vvenc_init_default(&cfg, width, height, 60, 0, 32, VVENC_SLOW);
  cfg.m_framesToBeEncoded   = 1;
  cfg.m_numThreads          = 0;
  cfg.m_verbosity           = VVENC_WARNING;
  cfg.m_RCTargetBitrate     = 0;
  cfg.m_internalBitDepth[0] = 8;
  cfg.m_internalBitDepth[1] = 8;
  cfg.m_internChromaFormat  = VVENC_CHROMA_420;
  if (withHw)
  {
    cfg.m_hwPreAnalysis = 1;
    if (hwMetadataPath)
      strncpy(cfg.m_hwMetadataPath, hwMetadataPath, sizeof(cfg.m_hwMetadataPath));
  }
  vvenc_set_msg_callback(&cfg, nullptr, msgCb);
  return cfg;
}

// ── Main ──────────────────────────────────────────────────────────
int main(int argc, char* argv[])
{
  bool withHw = false;
  bool showHelp = false;

  // Defaults, may be overridden by positional args
  const char* inputPath = g_inputYuv;
  const char* refPath   = g_refPath;

  for (int i = 1; i < argc; i++)
  {
    if (strcmp(argv[i], "--with-hw") == 0) withHw = true;
    else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) showHelp = true;
    else if (inputPath == g_inputYuv) inputPath = argv[i];
    else refPath = argv[i];
  }

  if (showHelp)
  {
    printf("Usage: hw_pipeline_bench [input.yuv] [ref.266] [--with-hw]\n");
    printf("  --with-hw   Generate HW metadata and enable HW pre-analysis\n");
    return 0;
  }

  // ── Load input ────────────────────────────────────────────────
  std::vector<uint8_t> yuvData;
  if (loadFile(inputPath, yuvData) != 0)
  {
    fprintf(stderr, "FATAL: input YUV not found: %s\n", inputPath);
    return 1;
  }
  const int width = 1920, height = 1080;
  if (yuvData.size() < (size_t)width * height * 3 / 2)
  {
    fprintf(stderr, "FATAL: input YUV too small\n"); return 1;
  }

  // ── Generate HW metadata if requested ─────────────────────────
  const char* hwCsv = nullptr;
  const char* hwBin = nullptr;
  if (withHw)
  {
    mkdir("/tmp/hw_pbench", 0755);
    hwCsv = "/tmp/hw_pbench/metadata.csv";
    hwBin = "/tmp/hw_pbench/metadata_grids.bin";
    if (genHWMetadata(width, height, 16, hwCsv, hwBin) != 0)
    {
      fprintf(stderr, "FATAL: cannot generate HW metadata\n"); return 1;
    }
    printf("HW metadata: %s + %s\n", hwCsv, hwBin);
  }

  // ── Run baseline encode ───────────────────────────────────────
  vvenc_config cfgOff = makeConfig(width, height, false, nullptr);

  vvencEncoder* enc = vvenc_encoder_create();
  if (!enc) { fprintf(stderr, "FATAL: create failed\n"); return 1; }
  int ret = vvenc_encoder_open(enc, &cfgOff);
  if (ret != 0) { fprintf(stderr, "FATAL: open: %d\n", ret); return 1; }

  std::vector<uint8_t> bitsOff;
  double msOff = 0;
  ret = runEncode(enc, width, height, yuvData.data(), bitsOff, msOff);
  vvenc_encoder_close(enc);
  if (ret != 0) return 1;

  printf("BASELINE: %zu bytes, %.0f ms\n", bitsOff.size(), msOff);

  // ── Run HW-on encode ─────────────────────────────────────────
  double msOn = 0;
  std::vector<uint8_t> bitsOn;

  if (withHw)
  {
    vvenc_config cfgOn = makeConfig(width, height, true, hwCsv);
    enc = vvenc_encoder_create();
    if (!enc) { fprintf(stderr, "FATAL: create failed\n"); return 1; }
    ret = vvenc_encoder_open(enc, &cfgOn);
    if (ret != 0) { fprintf(stderr, "FATAL: open: %d\n", ret); return 1; }

    ret = runEncode(enc, width, height, yuvData.data(), bitsOn, msOn);
    vvenc_encoder_close(enc);
    if (ret != 0) return 1;

    printf("HW-GUIDED: %zu bytes, %.0f ms\n", bitsOn.size(), msOn);
  }

  // ── Reference management ──────────────────────────────────────
  std::vector<uint8_t> refBits;
  bool hasRef = (loadFile(refPath, refBits) == 0);

  if (!hasRef)
  {
    if (saveFile(refPath, bitsOff.data(), bitsOff.size()) != 0)
    {
      fprintf(stderr, "FATAL: cannot write reference\n"); return 1;
    }
    printf("Reference: %s (%zu bytes) — CREATED\n", refPath, bitsOff.size());
    return 0;
  }

  // ── Verify baseline ───────────────────────────────────────────
  bool pass = (bitsOff.size() == refBits.size() &&
               memcmp(bitsOff.data(), refBits.data(), bitsOff.size()) == 0);
  printf("Bit-exact:  %s\n", pass ? "YES (PASS)" : "NO (FAIL)");

  // ── Print comparison ──────────────────────────────────────────
  if (withHw && msOn > 0 && msOff > 0)
  {
    double ratio = msOff / msOn;
    printf("\n=== A/B COMPARISON ===\n");
    printf("  Baseline:  %.0f ms\n", msOff);
    printf("  HW-guided: %.0f ms\n", msOn);
    printf("  Speedup:   %.2f x (%.0f%%)\n", ratio, (ratio - 1.0) * 100.0);
    if (bitsOn.size() != bitsOff.size())
    {
      double brRatio = (double)bitsOn.size() / bitsOff.size();
      printf("  Bitrate:   %+.1f%% (%zu vs %zu bytes)\n",
             (brRatio - 1.0) * 100.0, bitsOn.size(), bitsOff.size());
    }
  }

  if (!pass) return 1;
  return 0;
}
