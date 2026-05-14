#define USE_AVX2 1
#include <iostream>
#include <iomanip>
#include <random>
#include <cstdint>
#include <cstring>
#include <chrono>
#include "RdCost.h"
#include "x86/CommonDefX86.h"
#include "x86/RdCostX86.h"
using namespace vvenc;

extern "C" uint64_t vvenc_had8_avx2(const DistParam* dp);
extern "C" uint64_t vvenc_had16_avx2(const DistParam* dp);

static uint64_t cpp_ref(const DistParam& dp) {
  int w = dp.org.width, h = dp.org.height;
  if (w == 8 && h == 8)
    return xCalcHAD8x8_SSE(dp.org.buf, dp.cur.buf, dp.org.stride, dp.cur.stride, dp.bitDepth);
  if (w == 16 && h == 16)
    return xCalcHAD16x16_AVX2(dp.org.buf, dp.cur.buf, dp.org.stride, dp.cur.stride, dp.bitDepth);
  uint64_t sum = 0;
  int sO = dp.org.stride, sC = dp.cur.stride, bd = dp.bitDepth;
  auto pO = dp.org.buf, pC = dp.cur.buf;
  for (int y = 0; y < h; y += 8)
    for (int x = 0; x < w; x += 8)
      sum += xCalcHAD8x8_SSE(pO + x + y * sO, pC + x + y * sC, sO, sC, bd);
  return sum;
}

static uint64_t asm_ref(const DistParam& dp) {
  if (dp.org.width == 8)
    return vvenc_had8_avx2(&dp);
  if (dp.org.height < 16) {
    uint64_t s = 0;
    DistParam dp2 = dp;
    CPelBuf o2(dp2.org.buf, dp2.org.stride, 8, dp2.org.height);
    CPelBuf c2(dp2.cur.buf, dp2.cur.stride, 8, dp2.cur.height);
    dp2.org = o2; dp2.cur = c2;
    s += vvenc_had8_avx2(&dp2);
    dp2.org = CPelBuf(dp.org.buf + 8, dp.org.stride, 8, dp.org.height);
    dp2.cur = CPelBuf(dp.cur.buf + 8, dp.cur.stride, 8, dp.cur.height);
    s += vvenc_had8_avx2(&dp2);
    return s;
  }
  return vvenc_had16_avx2(&dp);
}

struct TestCase { int w, h, stride; const char* name; };
static const TestCase cases[] = {
  {8,  8,  16, "8x8"},
  {8,  16, 16, "8x16"},
  {8,  32, 16, "8x32"},
  {16, 16, 32, "16x16"},
  // {16, 8,  16, "16x8"},
};
static const int NUM_CASES = sizeof(cases) / sizeof(cases[0]);

int main(int argc, char** argv) {
  std::mt19937_64 rng(42);
  std::uniform_int_distribution<int> dist(0, 1023);
  const int BD = 10;
  bool bench_mode = (argc > 1 && strcmp(argv[1], "bench") == 0);

  const int MAX_STRIDE = 128;
  const int MAX_H = 64;
  Pel* org = new Pel[MAX_STRIDE * MAX_H];
  Pel* cur = new Pel[MAX_STRIDE * MAX_H];

  bool all_pass = true;
  for (int ci = 0; ci < NUM_CASES; ci++) {
    int w = cases[ci].w, h = cases[ci].h, stride = cases[ci].stride;
    for (int trial = 0; trial < 20; trial++) {
      for (int i = 0; i < stride * h; i++) {
        org[i] = (Pel)dist(rng);
        cur[i] = (Pel)dist(rng);
      }
      CPelBuf cp_org(org, stride, w, h);
      CPelBuf cp_cur(cur, stride, w, h);
      DistParam dp(cp_org, cp_cur, nullptr, BD, 0, COMP_Y);
      uint64_t r1 = cpp_ref(dp);
      uint64_t r2 = asm_ref(dp);
      if (r1 != r2) {
        std::cerr << "FAIL " << cases[ci].name << " trial " << trial
                  << ": C++=" << r1 << " ASM=" << r2 << "\n";
        all_pass = false;
      }
    }
    if (all_pass)
      std::cout << "PASS " << cases[ci].name << "\n";
  }

  if (bench_mode) {
    const int ITERS = 200000;
    std::cout << "\nBenchmark (" << ITERS << " iter each):\n";
    std::cout << std::left << std::setw(10) << "Case"
              << std::setw(16) << "C++ (ns)"
              << std::setw(16) << "ASM (ns)"
              << std::setw(14) << "Speedup\n"
              << std::string(56, '-') << "\n";

    for (int i = 0; i < MAX_STRIDE * MAX_H; i++) {
      org[i] = (Pel)dist(rng);
      cur[i] = (Pel)dist(rng);
    }

    for (int ci = 0; ci < NUM_CASES; ci++) {
      int w = cases[ci].w, h = cases[ci].h, stride = cases[ci].stride;
      CPelBuf cp_org(org, stride, w, h);
      CPelBuf cp_cur(cur, stride, w, h);
      DistParam dp(cp_org, cp_cur, nullptr, BD, 0, COMP_Y);

      // Warmup
      volatile uint64_t dummy = 0;
      for (int i = 0; i < 5000; i++) dummy += cpp_ref(dp) + asm_ref(dp);

      // Time C++
      auto t1 = std::chrono::high_resolution_clock::now();
      uint64_t s1 = 0;
      for (int i = 0; i < ITERS; i++) s1 += cpp_ref(dp);
      auto t2 = std::chrono::high_resolution_clock::now();
      double ns_cpp = std::chrono::duration<double, std::nano>(t2 - t1).count() / ITERS;

      // Time ASM
      t1 = std::chrono::high_resolution_clock::now();
      uint64_t s2 = 0;
      for (int i = 0; i < ITERS; i++) s2 += asm_ref(dp);
      t2 = std::chrono::high_resolution_clock::now();
      double ns_asm = std::chrono::duration<double, std::nano>(t2 - t1).count() / ITERS;

      double speedup = ns_cpp / ns_asm;

      std::cout << std::left << std::setw(10) << cases[ci].name
                << std::fixed << std::setprecision(2)
                << std::setw(16) << ns_cpp
                << std::setw(16) << ns_asm
                << std::setw(14) << speedup << "x\n";
      (void)dummy; (void)s1; (void)s2;
    }
  }

  delete[] org;
  delete[] cur;
  if (!all_pass) return 1;
  if (!bench_mode) std::cout << "\nAll bit-exact. Run with 'bench' for timing.\n";
  return 0;
}
