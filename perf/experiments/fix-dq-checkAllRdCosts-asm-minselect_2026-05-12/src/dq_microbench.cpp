#define USE_AVX2 1
#include <iostream>
#include <random>
#include <cstdint>
#include <cstring>
#include <limits>
#include <chrono>

#include "DepQuant.h"
#include "Contexts.h"
#include "x86/CommonDefX86.h"
#include "x86/DepQuantX86.h"

using namespace vvenc;

extern "C" void vvenc_dq_checkAllRdCosts_avx2(
    int spt, const DQIntern::PQData* pqData,
    DQIntern::Decisions* decisions, const DQIntern::StateMem* state);

static constexpr int64_t RD_COST_INIT = std::numeric_limits<int64_t>::max() >> 1;

struct BenchInput {
    DQIntern::ScanPosType   spt;
    DQIntern::PQData        pqData[4];
    DQIntern::Decisions     decisions;
    DQIntern::StateMem      state;
    BinFracBits sigFracBits[4][12];
    DQIntern::CoeffFracBits gtxFracBits[21];
};

static BenchInput inputs[16];

static void init_inputs() {
    std::mt19937_64 rng(42);

    for (int t = 0; t < 16; t++) {
        auto& in = inputs[t];
        in.spt = static_cast<DQIntern::ScanPosType>((t / 6) % 3);

        for (int i = 0; i < 4; i++) {
            int level = (int)(rng() % 7);
            in.pqData[i].absLevel  = (TCoeff)level;
            in.pqData[i].deltaDist = (int64_t)(rng() % 100000) - 50000;
        }

        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 12; j++) {
                in.sigFracBits[i][j].intBits[0] = 1 << 15;
                in.sigFracBits[i][j].intBits[1] = 1 << 15;
            }
        for (int j = 0; j < 21; j++)
            for (int k = 0; k < 6; k++)
                in.gtxFracBits[j].bits[k] = 1 << 15;

        for (int i = 0; i < 4; i++) {
            in.state.rdCost[i]      = (int64_t)(rng() % (RD_COST_INIT >> 2));
            in.state.remRegBins[i]  = 16;
            in.state.sbbBits0[i]    = 0;
            in.state.sbbBits1[i]    = (int32_t)(rng() % 2000 + 500);
            in.state.numSig[i]      = (uint8_t)(rng() % 4);
            in.state.ctx.sig[i]     = (uint8_t)(rng() % 12);
            in.state.ctx.cff[i]     = (uint8_t)(rng() % 21);
            in.state.refSbbCtxId[i] = 0;
            in.state.m_goRicePar[i] = 0;
            in.state.m_goRiceZero[i]= 0;

            for (int j = 0; j < 16; j++) {
                in.state.tplAcc[j][i] = (uint8_t)(rng() & 0xff);
                in.state.sum1st[j][i] = (uint8_t)(rng() & 0xff);
                in.state.absVal[j][i] = (uint8_t)(rng() & 0x7f);
            }
        }

        in.state.m_sigFracBitsArray[0] = &in.sigFracBits[0][0];
        in.state.m_sigFracBitsArray[1] = &in.sigFracBits[1][0];
        in.state.m_sigFracBitsArray[2] = &in.sigFracBits[2][0];
        in.state.m_sigFracBitsArray[3] = &in.sigFracBits[3][0];
        in.state.m_gtxFracBitsArray    = in.gtxFracBits;
        in.state.cffBitsCtxOffset      = 0;
        in.state.anyRemRegBinsLt4      = false;
        in.state.initRemRegBins        = 0;

        for (int i = 0; i < 4; i++) {
            in.decisions.rdCost[i]   = RD_COST_INIT;
            in.decisions.absLevel[i] = 0;
            in.decisions.prevId[i]   = -2;
        }
    }
}

static bool compare() {
    bool ok = true;
    for (int t = 0; t < 16; t++) {
        auto& in = inputs[t];
        DQIntern::Decisions ref;
        DQInternSimd::checkAllRdCosts<AVX2>(in.spt, in.pqData, ref, in.state);

        DQIntern::Decisions asm_res;
        vvenc_dq_checkAllRdCosts_avx2(in.spt, in.pqData, &asm_res, &in.state);

        for (int i = 0; i < 4; i++) {
            if (ref.rdCost[i] != asm_res.rdCost[i] ||
                ref.absLevel[i] != asm_res.absLevel[i] ||
                ref.prevId[i] != asm_res.prevId[i]) {
                std::cerr << "MISMATCH test[" << t << "] state[" << i << "]: "
                          << "spt=" << (int)in.spt << " "
                          << "rdCost ref=" << ref.rdCost[i] << " asm=" << asm_res.rdCost[i] << " "
                          << "absLevel ref=" << (int)ref.absLevel[i] << " asm=" << (int)asm_res.absLevel[i] << " "
                          << "prevId ref=" << (int)ref.prevId[i] << " asm=" << (int)asm_res.prevId[i] << "\n";
                ok = false;
            }
        }
    }
    return ok;
}

// Force indirect call through volatile pointer to prevent inlining
static void(* volatile g_pfn_asm)(int, const DQIntern::PQData*, DQIntern::Decisions*, const DQIntern::StateMem*) =
    vvenc_dq_checkAllRdCosts_avx2;

__attribute__((noinline))
static void bench_asm(int iterations) {
    for (int iter = 0; iter < iterations; iter++) {
        auto& in = inputs[iter & 0xf];
        DQIntern::Decisions dec;
        g_pfn_asm(in.spt, in.pqData, &dec, &in.state);
        __asm__ volatile("" : "+m" (dec));
    }
}

// Indirect call for C++ too — cast the function reference through a volatile pointer
static void(* volatile g_pfn_cpp)(DQIntern::ScanPosType, const DQIntern::PQData*, DQIntern::Decisions&, const DQIntern::StateMem&) =
    DQInternSimd::checkAllRdCosts<AVX2>;

__attribute__((noinline))
static void bench_cpp(int iterations) {
    for (int iter = 0; iter < iterations; iter++) {
        auto& in = inputs[iter & 0xf];
        DQIntern::Decisions dec;
        g_pfn_cpp(in.spt, in.pqData, dec, in.state);
        __asm__ volatile("" : "+m" (dec));
    }
}

int main(int argc, char** argv) {
    init_inputs();

    if (!compare()) {
        std::cerr << "MISMATCHES DETECTED\n";
        return 1;
    }
    std::cerr << "Bit-exact: YES\n";

    int iterations = 2000000;
    if (argc > 1) iterations = atoi(argv[1]);

    std::cerr << "Warming up...\n";
    bench_asm(100000);

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
