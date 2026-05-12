#include "asm-primitives.h"

namespace vvenc {

void setupAssemblyPrimitives(VVencPrimitive& p, int cpuMask)
{
  (void)p;
  (void)cpuMask;

  // Phase 2 — register assembly function pointers here:
  //
  // NASM/YASM functions are declared in asm-primitives.h with
  // extern "C" linkage and registered by name:
  //
  // if (cpuMask & VVENC_CPU_AVX2) {
  //     p.dist.hadamard4x4 = vvenc_hadamard_4x4_avx2;
  //     // ...
  // }
}

} // namespace vvenc
