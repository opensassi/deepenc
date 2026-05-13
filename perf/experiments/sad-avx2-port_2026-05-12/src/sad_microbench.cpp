#define USE_AVX2 1
#include <iostream>
#include <random>
#include <cstdint>
#include <cstring>
#include <limits>
#include <chrono>
#include <vector>
#include <string>
#include <algorithm>

#include "RdCost.h"
#include "x86/CommonDefX86.h"
#include "x86/RdCostX86.h"

using namespace vvenc;

// ============================================================
// ASM function declarations
// ============================================================
// ASM function declarations — defined in libvvenc.a (avx2/asm-sad_avx2.cpp)
extern "C" uint64_t vvenc_sad8_avx2(const DistParam* dp);
extern "C" uint64_t vvenc_sad16_avx2(const DistParam* dp);
extern "C" uint64_t vvenc_sad32_avx2(const DistParam* dp);
extern "C" uint64_t vvenc_sad64_avx2(const DistParam* dp);

// ============================================================
// Test configuration
// ============================================================
struct BenchConfig {
    int width;
    int height;
    std::string label;
};

static const BenchConfig testConfigs[] = {
    { 8,  8,  "SAD8x8"   },
    { 16, 16, "SAD16x16" },
    { 32, 32, "SAD32x32" },
    { 64, 64, "SAD64x64" },
    { 8,  16, "SAD8x16"  },
    { 16, 8,  "SAD16x8"  },
    { 32, 16, "SAD32x16" },
    { 64, 32, "SAD64x32" },
};
static constexpr int NUM_CONFIGS = sizeof(testConfigs) / sizeof(testConfigs[0]);

// ============================================================
// Test input buffers — one per block size
// ============================================================
static constexpr int ALIGNMENT = 32;
static constexpr int MAX_SIZE = 128;
static constexpr int STRIDE_PAD = 8;

struct TestInput {
    std::vector<Pel> orgBuf;
    std::vector<Pel> curBuf;
    int stride;
    int bitDepth;
    int subShift;
};

static TestInput inputs[NUM_CONFIGS];

static void init_inputs() {
    std::mt19937_64 rng(42);
    std::uniform_int_distribution<int> dist(0, 1023);

    for (int ci = 0; ci < NUM_CONFIGS; ci++) {
        auto& cfg = testConfigs[ci];
        auto& in = inputs[ci];

        in.stride = cfg.width + STRIDE_PAD;
        in.bitDepth = 10;
        in.subShift = 0;

        int totalPels = in.stride * cfg.height;
        in.orgBuf.resize(totalPels);
        in.curBuf.resize(totalPels);

        for (int i = 0; i < totalPels; i++) {
            in.orgBuf[i] = (Pel)dist(rng);
            in.curBuf[i] = (Pel)dist(rng);
        }
    }
}

// ============================================================
// Build DistParam for a given config index
// ============================================================
static DistParam makeDistParam(int ci) {
    auto& cfg = testConfigs[ci];
    auto& in = inputs[ci];

    CPelBuf org(in.orgBuf.data(), in.stride, cfg.width, cfg.height);
    CPelBuf cur(in.curBuf.data(), in.stride, cfg.width, cfg.height);

    return DistParam(org, cur, nullptr, in.bitDepth, in.subShift, COMP_Y);
}

// ============================================================
// C++ SIMD reference function pointers (via volatile)
// ============================================================

// For width-specific SAD (NxN templates)
template<int W>
using SadFunc = Distortion (*)(const DistParam&);

template<int W>
static SadFunc<W> cppSadFn() {
    return RdCost::xGetSAD_NxN_SIMD<W, AVX2>;
}

// volatile pointers to prevent inlining
template<int W>
static SadFunc<W> volatile g_pfn_cpp_sad = RdCost::xGetSAD_NxN_SIMD<W, AVX2>;

// ASM function pointers
using AsmSadFunc = uint64_t (*)(const DistParam*);
static AsmSadFunc volatile g_pfn_asm_sad8  = vvenc_sad8_avx2;
static AsmSadFunc volatile g_pfn_asm_sad16 = vvenc_sad16_avx2;
static AsmSadFunc volatile g_pfn_asm_sad32 = vvenc_sad32_avx2;
static AsmSadFunc volatile g_pfn_asm_sad64 = vvenc_sad64_avx2;

static AsmSadFunc volatile asmFnForWidth(int w) {
    switch (w) {
        case 8:  return g_pfn_asm_sad8;
        case 16: return g_pfn_asm_sad16;
        case 32: return g_pfn_asm_sad32;
        case 64: return g_pfn_asm_sad64;
        default: return nullptr;
    }
}

// ============================================================
// Bit-exact comparison helpers
// ============================================================
template<int W>
static bool checkOne() {
    auto pfn_asm = asmFnForWidth(W);
    for (int ci = 0; ci < NUM_CONFIGS; ci++) {
        if (testConfigs[ci].width != W) continue;
        auto dp = makeDistParam(ci);

        Distortion ref = RdCost::xGetSAD_NxN_SIMD<W, AVX2>(dp);
        Distortion asm_ = pfn_asm(&dp);

        if (ref != asm_) {
            std::cerr << "  MISMATCH " << testConfigs[ci].label
                      << ": ref=" << ref << " asm=" << asm_ << "\n";
            return false;
        }
    }
    return true;
}

template<int W>
static void benchOne(int iterations, double& cpp_ms, double& asm_ms) {
    DistParam dps[NUM_CONFIGS];
    for (int ci = 0; ci < NUM_CONFIGS; ci++) {
        dps[ci] = makeDistParam(ci);
    }
    int numDps = 0;
    for (int ci = 0; ci < NUM_CONFIGS; ci++) {
        if (testConfigs[ci].width == W) {
            dps[numDps++] = dps[ci];
        }
    }
    if (numDps == 0) return;
    int numDpsLocal = numDps;

    volatile Distortion sink = 0;

    // C++ measurement
    volatile auto pfn_cpp = g_pfn_cpp_sad<W>;
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int iter = 0; iter < iterations; iter++) {
        const auto& dp = dps[iter % numDpsLocal];
        sink += pfn_cpp(dp);
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    cpp_ms = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() / 1000.0;

    // ASM measurement
    volatile auto pfn_asm = asmFnForWidth(W);
    auto t2 = std::chrono::high_resolution_clock::now();
    for (int iter = 0; iter < iterations; iter++) {
        const auto& dp = dps[iter % numDpsLocal];
        sink += pfn_asm(&dp);
    }
    auto t3 = std::chrono::high_resolution_clock::now();
    asm_ms = std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2).count() / 1000.0;

    (void)sink;
}

// ============================================================
// Per-size benchmark runner
// ============================================================
template<int W>
static void runBench(int iterations) {
    std::cerr << "  [" << W << "xN]\n";

    double cpp_ms, asm_ms;
    benchOne<W>(iterations, cpp_ms, asm_ms);

    double cpp_iter_s = (iterations * 1000.0) / cpp_ms;
    double cpp_ns = (cpp_ms * 1e6) / iterations;

    std::cerr.precision(3);
    std::cerr << std::fixed;
    std::cerr << "    C++:  " << cpp_ms << " ms  "
              << cpp_iter_s / 1e6 << " M iter/s  "
              << cpp_ns << " ns/call\n";

    double asm_iter_s = (iterations * 1000.0) / asm_ms;
    double asm_ns = (asm_ms * 1e6) / iterations;
    double speedup = cpp_ms / asm_ms;
    std::cerr << "    ASM:  " << asm_ms << " ms  "
              << asm_iter_s / 1e6 << " M iter/s  "
              << asm_ns << " ns/call  "
              << "speedup: " << speedup << "x\n";
}

// ============================================================
// Check all configurations for bit-exactness
// ============================================================
template<int W>
static bool checkWidth() {
    bool ok = checkOne<W>();
    if (ok) {
        std::cerr << "  [" << W << "xN] bit-exact: YES\n";
    } else {
        std::cerr << "  [" << W << "xN] bit-exact: FAIL\n";
    }
    return ok;
}

// ============================================================
// main
// ============================================================
int main(int argc, char** argv) {
    init_inputs();

    int iterations = 2000000;
    if (argc > 1) iterations = atoi(argv[1]);

    std::cerr << "Warming up...\n";
    // warm up
    for (int i = 0; i < 100000; i++) {
        auto dp = makeDistParam(i % NUM_CONFIGS);
        volatile auto pfn = g_pfn_cpp_sad<16>;
        volatile Distortion s = pfn(dp);
        (void)s;
    }

    std::cerr << "Bit-exactness check:\n";
    bool allOk = true;
    allOk &= checkWidth<8>();
    allOk &= checkWidth<16>();
    allOk &= checkWidth<32>();
    allOk &= checkWidth<64>();

    if (!allOk) {
        std::cerr << "FAIL: bit-exact mismatches detected\n";
        return 1;
    }

    std::cerr << "\nBenchmarking " << iterations << " iterations...\n";
    std::cerr << "C++ SIMD (AVX2) baseline:\n";
    runBench<8>(iterations);
    runBench<16>(iterations);
    runBench<32>(iterations);
    runBench<64>(iterations);

    std::cerr << "\nDone.\n";
    return 0;
}
