#include "asm-primitives.h"

namespace vvenc {

void setupAssemblyPrimitives(VVencPrimitive& p, int cpuMask)
{
  (void)p;
  (void)cpuMask;

#if defined(USE_AVX2)
  p.interp.filterHor[0][0][0] = reinterpret_cast<decltype(p.interp.filterHor[0][0][0])>(vvenc_interp_horiz_8tap_avx2);
  p.interp.filterVer[0][0][0] = reinterpret_cast<decltype(p.interp.filterVer[0][0][0])>(vvenc_interp_vert_8tap_avx2);
#endif
}

} // namespace vvenc
