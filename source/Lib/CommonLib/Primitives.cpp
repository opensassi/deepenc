#include "Primitives.h"
#include "CommonDefX86.h"

namespace vvenc {

// Global singleton
VVencPrimitive g_vvenc;

// ---------------------------------------------------------------------------
// setupCPrimitives — populate all function pointers with C fallbacks
//
// Called first. Every entry is assigned a portable C implementation.
// Assembly or intrinsic overrides replace entries in setupAssemblyPrimitives.
// ---------------------------------------------------------------------------

void setupCPrimitives(VVencPrimitive& p)
{
  (void)p;

  // Phase 1 — populate from existing C++ implementations:
  //
  // setupPelBufferOpsC(p);
  // setupInterpolationFilterC(p);
  // setupRdCostC(p);
  // setupIntraPredictionC(p);
  // setupDQTablesC(p);
  // ...
}

// ---------------------------------------------------------------------------
// setupAliasPrimitives — create aliases for HBD / chroma variants
//
// Maps existing primitives to additional configurations
// (e.g., 10-bit pixel uses same function as 8-bit via type-shift).
// ---------------------------------------------------------------------------

void setupAliasPrimitives(VVencPrimitive& p)
{
  (void)p;

  // Phase 3 — alias setup:
  //
  // p.chroma_pu.satd = p.luma_pu.satd;   // chroma reuses luma SATD
  // p.cu_10bit.dct = p.cu_8bit.dct;      // HBD reuses via cast
}

// ---------------------------------------------------------------------------
// vvenc_setup_primitives — main entry point
//
// Called once at encoder startup after CPU detection.
// Order: C refs → assembly/intrinsic → alias
// ---------------------------------------------------------------------------

void vvenc_setup_primitives(int cpuMask)
{
  // Skip if already initialized (idempotent)
  static bool s_initialized = false;
  if (s_initialized) return;
  s_initialized = true;

  // 1. Populate C reference implementations
  setupCPrimitives(g_vvenc);

  // 2. Override with assembly (NASM) or intrinsic implementations
  setupAssemblyPrimitives(g_vvenc, cpuMask);

  // 3. Create aliases for HBD / chroma
  setupAliasPrimitives(g_vvenc);
}

} // namespace vvenc
