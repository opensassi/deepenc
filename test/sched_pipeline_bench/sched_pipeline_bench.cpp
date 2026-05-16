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

// ── Scheduler globals ───────────────────────────────────────────
namespace vvenc {
void vvencSetSchedulerDisabled(bool disabled);
extern int g_schedulerDispatchCount;
}

// ── Paths ────────────────────────────────────────────────────────
static const char* g_inputYuv     = "perf/traces/sched_pipeline_baseline/input.yuv";
static const char* g_refPath      = "perf/traces/sched_pipeline_baseline/output.266";

// ── Logging callback ─────────────────────────────────────────────
static void msgCb(void*, int level, const char* fmt, va_list args)
{
  if (level <= 1) vfprintf(stderr, fmt, args);
}

// ── File I/O ─────────────────────────────────────────────────────
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

// ── Fill YUV buffer ─────────────────────────────────────────────
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

// ── Run encode ───────────────────────────────────────────────────
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

// ── Configure encoder ────────────────────────────────────────────
static vvenc_config makeConfig(int width, int height)
{
  // Exact match for vvencapp --preset slow --qp 32 -f 1 --threads 0:
  // 1) init_default with MEDIUM preset
  // 2) init_preset with SLOW
  // 3) override QP/frames/threads via set_param
  vvenc_config cfg;
  vvenc_init_default(&cfg, width, height, 60, VVENC_RC_OFF, VVENC_AUTO_QP, VVENC_MEDIUM);
  vvenc_init_preset(&cfg, VVENC_SLOW);
  cfg.m_QP                  = 32;
  cfg.m_framesToBeEncoded   = 1;
  cfg.m_numThreads          = 0;
  cfg.m_verbosity           = VVENC_WARNING;
  vvenc_set_msg_callback(&cfg, nullptr, msgCb);
  return cfg;
}

// ── Main ─────────────────────────────────────────────────────────
int main(int argc, char* argv[])
{
  bool withSched = false;
  bool showHelp = false;

  const char* inputPath = g_inputYuv;
  const char* refPath   = g_refPath;

  for (int i = 1; i < argc; i++)
  {
    if (strcmp(argv[i], "--with-sched") == 0) withSched = true;
    else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) showHelp = true;
    else if (inputPath == g_inputYuv) inputPath = argv[i];
    else refPath = argv[i];
  }

  if (showHelp)
  {
    printf("Usage: sched_pipeline_bench [input.yuv] [ref.266] [--with-sched]\n");
    printf("  --with-sched   Enable scheduler dispatch (A/B comparison)\n");
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

  // ── Run encode (one per invocation — multi-encode in one process
  //    corrupts VVenC global state) ───────────────────────────────
  vvenc::vvencSetSchedulerDisabled(!withSched);
  vvenc_config cfg = makeConfig(width, height);

  vvencEncoder* enc = vvenc_encoder_create();
  if (!enc) { fprintf(stderr, "FATAL: create failed\n"); return 1; }
  int ret = vvenc_encoder_open(enc, &cfg);
  if (ret != 0) { fprintf(stderr, "FATAL: open: %d\n", ret); return 1; }

  std::vector<uint8_t> bits;
  double ms = 0;
  ret = runEncode(enc, width, height, yuvData.data(), bits, ms);
  vvenc_encoder_close(enc);
  if (ret != 0) return 1;

  printf("%s: %zu bytes, %.0f ms, dispatch_hits=%d\n",
         withSched ? "SCHED" : "BASELINE",
         bits.size(), ms, vvenc::g_schedulerDispatchCount);

  // ── Reference management ──────────────────────────────────────
  std::vector<uint8_t> refBits;
  bool hasRef = (loadFile(refPath, refBits) == 0);

  if (!hasRef)
  {
    if (saveFile(refPath, bits.data(), bits.size()) != 0)
    {
      fprintf(stderr, "FATAL: cannot write reference\n"); return 1;
    }
    printf("Reference: %s (%zu bytes) — CREATED\n", refPath, bits.size());
    return 0;
  }

  // ── Verify bit-exactness ─────────────────────────────────────
  bool pass = (bits.size() == refBits.size() &&
               memcmp(bits.data(), refBits.data(), bits.size()) == 0);
  printf("Bit-exact:  %s\n", pass ? "YES (PASS)" : "NO (FAIL)");

  if (!pass) return 1;
  return 0;
}
