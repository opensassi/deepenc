#pragma once

#include "Primitives.h"

namespace vvenc {

// Forward declarations for NASM/YASM assembly functions.
//
// Each asm function is declared here with C linkage so the linker
// can resolve the NASM symbol (mangled via x265-style PFX macro).
//
// Registration happens in setupAssemblyPrimitives() (asm-primitives.cpp).
//
// File naming convention (matching x265 pattern):
//   source/Lib/CommonLib/x86/<operation>.asm
//
// Example:
//   extern "C" void PFX(hadamard_4x4_avx2)(...);
//
// Phase 2 — add asm function declarations here as implementations
//            are ported from C++ intrinsics.

// Horizontal 8-tap interpolation filter (AVX2)
// Signature matches: interp.filterHor[0][0][0] — 8-tap, isFirst=false, isLast=false
extern "C" void vvenc_interp_horiz_8tap_avx2(
    const void* clpRng, const void* src, int srcStride,
    void* dst, int dstStride, int width, int height,
    const void* coeff);

// Vertical 8-tap interpolation filter (AVX2)
// Signature matches: interp.filterVer[0][0][0] — 8-tap, isFirst=false, isLast=false
extern "C" void vvenc_interp_vert_8tap_avx2(
    const void* clpRng, const void* src, int srcStride,
    void* dst, int dstStride, int width, int height,
    const void* coeff);

} // namespace vvenc
