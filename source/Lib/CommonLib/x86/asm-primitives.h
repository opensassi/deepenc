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

} // namespace vvenc
