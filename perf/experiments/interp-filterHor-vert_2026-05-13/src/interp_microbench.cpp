#define USE_AVX2 1
#include <iostream>
#include <random>
#include <cstdint>
#include <cstring>
#include <chrono>

#include "CommonDef.h"
#include "InterpolationFilter.h"
#include "x86/InterpolationFilterX86.h"
#include "x86/asm-primitives.h"

using namespace vvenc;

extern "C" void vvenc_interp_horiz_8tap_avx2(
    const void* clpRng, const void* src, int srcStride,
    void* dst, int dstStride, int width, int height,
    const void* coeff);

static const int WIDTH = 64;
static const int HEIGHT = 64;
static const int STRIDE = WIDTH + 16;
static int16_t srcBuf[HEIGHT * STRIDE];
static int16_t dstCpp[HEIGHT * WIDTH];
static int16_t dstAsm[HEIGHT * WIDTH];

static const int16_t testCoeff[8] = { -1, 4, -10, 58, 17, -5, 1, 0 };

static void init_inputs() {
    std::mt19937_64 rng(42);
    for (int i = 0; i < HEIGHT * STRIDE; i++) {
        srcBuf[i] = (int16_t)(rng() & 0x3FF);
    }
}

static bool compare() {
    bool ok = true;
    ClpRng clpRng;
    clpRng.bd = 10;

    simdFilter<AVX2, 8, false, false, false>(
        clpRng, srcBuf + 8, STRIDE, dstCpp, WIDTH, WIDTH, HEIGHT, testCoeff);

    vvenc_interp_horiz_8tap_avx2(
        &clpRng, srcBuf + 8, STRIDE, dstAsm, WIDTH, WIDTH, HEIGHT, testCoeff);

    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            int idx = y * WIDTH + x;
            if (dstCpp[idx] != dstAsm[idx]) {
                std::cerr << "MISMATCH at [" << y << "][" << x << "]: "
                          << "C++=" << dstCpp[idx] << " ASM=" << dstAsm[idx] << "\n";
                ok = false;
            }
        }
    }
    return ok;
}

static void(*volatile g_pfn_asm)(const void*, const void*, int, void*, int, int, int, const void*) =
    vvenc_interp_horiz_8tap_avx2;

__attribute__((noinline))
static void bench_asm(int iterations) {
    ClpRng clpRng;
    clpRng.bd = 10;
    for (int i = 0; i < iterations; i++) {
        g_pfn_asm(&clpRng, srcBuf + 8, STRIDE,
                  dstAsm, WIDTH, WIDTH, HEIGHT, testCoeff);
        __asm__ volatile("" : "+m" (dstAsm));
    }
}

__attribute__((noinline))
static void bench_cpp(int iterations) {
    ClpRng clpRng;
    clpRng.bd = 10;
    for (int i = 0; i < iterations; i++) {
        simdFilter<AVX2, 8, false, false, false>(
            clpRng, srcBuf + 8, STRIDE, dstCpp, WIDTH, WIDTH, HEIGHT, testCoeff);
        __asm__ volatile("" : "+m" (dstCpp));
    }
}

int main(int argc, char** argv) {
    init_inputs();

    if (!compare()) {
        std::cerr << "BIT-EXACT: FAIL\n";
        return 1;
    }
    std::cerr << "BIT-EXACT: YES\n";

    int iterations = 100000;
    if (argc > 1) iterations = atoi(argv[1]);

    std::cerr << "Warming up...\n";
    bench_asm(10000);

    std::cerr << "Benchmarking " << iterations << " iterations...\n";

    auto t0 = std::chrono::high_resolution_clock::now();
    bench_cpp(iterations);
    auto t1 = std::chrono::high_resolution_clock::now();
    bench_asm(iterations);
    auto t2 = std::chrono::high_resolution_clock::now();

    double cpp_ms = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() / 1000.0;
    double asm_ms = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count() / 1000.0;

    std::cerr.precision(3);
    std::cerr << std::fixed;
    std::cerr << "C++ SIMD ref: " << cpp_ms << " ms  (" << (iterations * 1000.0 / cpp_ms) << " iter/s)\n";
    std::cerr << "ASM:          " << asm_ms << " ms  (" << (iterations * 1000.0 / asm_ms) << " iter/s)\n";
    std::cerr << "Speedup:      " << (cpp_ms / asm_ms) << "x\n";

    return 0;
}
