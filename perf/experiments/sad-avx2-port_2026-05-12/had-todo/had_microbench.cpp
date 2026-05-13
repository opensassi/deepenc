#define USE_AVX2 1
#include <iostream>
#include <random>
#include <cstdint>
#include <chrono>
#include "RdCost.h"
#include "x86/CommonDefX86.h"
#include "x86/RdCostX86.h"
using namespace vvenc;

// No-array C++ version matching ASM structure
static uint32_t had8x8_opt(const int16_t* piOrg, const int16_t* piCur,
                            int strideOrg, int strideCur, int bitDepth) {
    // Load 8 rows, diff
    __m128i d0 = _mm_sub_epi16(_mm_loadu_si128((const __m128i*)(piOrg + 0*strideOrg)),
                                _mm_loadu_si128((const __m128i*)(piCur + 0*strideCur)));
    __m128i d1 = _mm_sub_epi16(_mm_loadu_si128((const __m128i*)(piOrg + 1*strideOrg)),
                                _mm_loadu_si128((const __m128i*)(piCur + 1*strideCur)));
    __m128i d2 = _mm_sub_epi16(_mm_loadu_si128((const __m128i*)(piOrg + 2*strideOrg)),
                                _mm_loadu_si128((const __m128i*)(piCur + 2*strideCur)));
    __m128i d3 = _mm_sub_epi16(_mm_loadu_si128((const __m128i*)(piOrg + 3*strideOrg)),
                                _mm_loadu_si128((const __m128i*)(piCur + 3*strideCur)));
    __m128i d4 = _mm_sub_epi16(_mm_loadu_si128((const __m128i*)(piOrg + 4*strideOrg)),
                                _mm_loadu_si128((const __m128i*)(piCur + 4*strideCur)));
    __m128i d5 = _mm_sub_epi16(_mm_loadu_si128((const __m128i*)(piOrg + 5*strideOrg)),
                                _mm_loadu_si128((const __m128i*)(piCur + 5*strideCur)));
    __m128i d6 = _mm_sub_epi16(_mm_loadu_si128((const __m128i*)(piOrg + 6*strideOrg)),
                                _mm_loadu_si128((const __m128i*)(piCur + 6*strideCur)));
    __m128i d7 = _mm_sub_epi16(_mm_loadu_si128((const __m128i*)(piOrg + 7*strideOrg)),
                                _mm_loadu_si128((const __m128i*)(piCur + 7*strideCur)));
    // Vertical butterfly stage 1
    __m128i s0 = _mm_add_epi16(d0, d4), s1 = _mm_add_epi16(d1, d5);
    __m128i s2 = _mm_add_epi16(d2, d6), s3 = _mm_add_epi16(d3, d7);
    __m128i s4 = _mm_sub_epi16(d0, d4), s5 = _mm_sub_epi16(d1, d5);
    __m128i s6 = _mm_sub_epi16(d2, d6), s7 = _mm_sub_epi16(d3, d7);
    // Stage 2
    d0 = _mm_add_epi16(s0, s2); d1 = _mm_add_epi16(s1, s3);
    d2 = _mm_sub_epi16(s0, s2); d3 = _mm_sub_epi16(s1, s3);
    d4 = _mm_add_epi16(s4, s6); d5 = _mm_add_epi16(s5, s7);
    d6 = _mm_sub_epi16(s4, s6); d7 = _mm_sub_epi16(s5, s7);
    // Stage 3
    s0 = _mm_add_epi16(d0, d1); s1 = _mm_sub_epi16(d0, d1);
    s2 = _mm_add_epi16(d2, d3); s3 = _mm_sub_epi16(d2, d3);
    s4 = _mm_add_epi16(d4, d5); s5 = _mm_sub_epi16(d4, d5);
    s6 = _mm_add_epi16(d6, d7); s7 = _mm_sub_epi16(d6, d7);
    // Transpose
    __m128i t0 = _mm_unpacklo_epi16(s0, s1), t1 = _mm_unpacklo_epi16(s2, s3);
    __m128i t2 = _mm_unpacklo_epi16(s4, s5), t3 = _mm_unpacklo_epi16(s6, s7);
    __m128i t4 = _mm_unpackhi_epi16(s0, s1), t5 = _mm_unpackhi_epi16(s2, s3);
    __m128i t6 = _mm_unpackhi_epi16(s4, s5), t7 = _mm_unpackhi_epi16(s6, s7);
    s0 = _mm_unpacklo_epi32(t0, t1); s1 = _mm_unpackhi_epi32(t0, t1);
    s2 = _mm_unpacklo_epi32(t2, t3); s3 = _mm_unpackhi_epi32(t2, t3);
    s4 = _mm_unpacklo_epi32(t4, t5); s5 = _mm_unpackhi_epi32(t4, t5);
    s6 = _mm_unpacklo_epi32(t6, t7); s7 = _mm_unpackhi_epi32(t6, t7);
    d0 = _mm_unpacklo_epi64(s0, s2); d1 = _mm_unpackhi_epi64(s0, s2);
    d2 = _mm_unpacklo_epi64(s1, s3); d3 = _mm_unpackhi_epi64(s1, s3);
    d4 = _mm_unpacklo_epi64(s4, s6); d5 = _mm_unpackhi_epi64(s4, s6);
    d6 = _mm_unpacklo_epi64(s5, s7); d7 = _mm_unpackhi_epi64(s5, s7);
    // d0-d7 = transposed columns (8 words each)

    // Sign-extend: lo and hi halves
    __m128i l0 = _mm_cvtepi16_epi32(d0), h0 = _mm_cvtepi16_epi32(_mm_srli_si128(d0, 8));
    __m128i l1 = _mm_cvtepi16_epi32(d1), h1 = _mm_cvtepi16_epi32(_mm_srli_si128(d1, 8));
    __m128i l2 = _mm_cvtepi16_epi32(d2), h2 = _mm_cvtepi16_epi32(_mm_srli_si128(d2, 8));
    __m128i l3 = _mm_cvtepi16_epi32(d3), h3 = _mm_cvtepi16_epi32(_mm_srli_si128(d3, 8));
    __m128i l4 = _mm_cvtepi16_epi32(d4), h4 = _mm_cvtepi16_epi32(_mm_srli_si128(d4, 8));
    __m128i l5 = _mm_cvtepi16_epi32(d5), h5 = _mm_cvtepi16_epi32(_mm_srli_si128(d5, 8));
    __m128i l6 = _mm_cvtepi16_epi32(d6), h6 = _mm_cvtepi16_epi32(_mm_srli_si128(d6, 8));
    __m128i l7 = _mm_cvtepi16_epi32(d7), h7 = _mm_cvtepi16_epi32(_mm_srli_si128(d7, 8));

    // Reorganize: g0 = lo[0-3], hi[0-3]; g1 = lo[4-7], hi[4-7]
    __m128i g0[8] = {l0, l1, l2, l3, h0, h1, h2, h3};
    __m128i g1[8] = {l4, l5, l6, l7, h4, h5, h6, h7};

    // Butterfly helper (inline)
    auto butterfly = [](__m128i* x) {
        __m128i a = _mm_add_epi32(x[0], x[4]), b = _mm_add_epi32(x[1], x[5]);
        __m128i c = _mm_add_epi32(x[2], x[6]), d = _mm_add_epi32(x[3], x[7]);
        __m128i e = _mm_sub_epi32(x[0], x[4]), f = _mm_sub_epi32(x[1], x[5]);
        __m128i g = _mm_sub_epi32(x[2], x[6]), h = _mm_sub_epi32(x[3], x[7]);
        // Stage 2
        x[0] = _mm_add_epi32(a, c); x[1] = _mm_add_epi32(b, d);
        x[2] = _mm_sub_epi32(a, c); x[3] = _mm_sub_epi32(b, d);
        x[4] = _mm_add_epi32(e, g); x[5] = _mm_add_epi32(f, h);
        x[6] = _mm_sub_epi32(e, g); x[7] = _mm_sub_epi32(f, h);
        // Stage 3 + abs
        x[0] = _mm_abs_epi32(_mm_add_epi32(x[0], x[1]));
        x[1] = _mm_abs_epi32(_mm_sub_epi32(x[0], x[1]));  // WAIT: x[0] was just modified!
    };

    // Oops, modified x[0] above in the abs step before using it. Need separate variables.
    // Let me redo this properly.

    return xCalcHAD8x8_SSE(piOrg, piCur, strideOrg, strideCur, bitDepth); // placeholder
}

int main(int argc, char** argv) {
    std::mt19937_64 rng(42);
    std::uniform_int_distribution<int> dist(0, 1023);
    const int W = 8, H = 8, STRIDE = 16, BD = 10;
    Pel org[STRIDE * H], cur[STRIDE * H];
    for (int i = 0; i < STRIDE * H; i++) {
        org[i] = (Pel)dist(rng);
        cur[i] = (Pel)dist(rng);
    }
    uint32_t ref = xCalcHAD8x8_SSE(org, cur, STRIDE, STRIDE, BD);
    std::cerr << "Ref: " << ref << "\n";
    return 0;
}
