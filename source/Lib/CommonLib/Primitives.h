#pragma once

#include "CommonDef.h"
#include "Common.h"

// Forward declarations for template types used in function pointer signatures.
template<typename T> struct AreaBuf;
template<typename T> struct UnitBuf;

namespace vvenc {

// Forward declarations for nested types used in function pointer signatures.
class DistParam;
class AlfClassifier;
struct AlfFilterShape;

namespace DQIntern {
  enum ScanPosType : int8_t;
  struct ScanInfo;
  struct TUParameters;
  struct PQData;
  struct Decisions;
  struct StateMem;
  struct CommonCtx;
}

const int VVENC_DF_TOTAL_FUNCTIONS = 34;

// ---------------------------------------------------------------------------
// VVencPrimitive — central SIMD/primitive dispatch table
//
// All function pointer tables for CPU-intensive operations. Each sub-struct
// mirrors the per-instance m_* / function pointer members in the module
// header. Populated by syncToGlobal() calls (Phase 1) and assembly
// registration through setupAssemblyPrimitives() (Phase 2).
// Dispatch switch from per-instance to g_vvenc is Phase 3.
// ---------------------------------------------------------------------------

struct VVencPrimitive
{
  // ------------------------------------------------------------------
  // 1. InterpolationFilter (InterpolationFilter.h, ~3.0% of perf)
  // ------------------------------------------------------------------
  struct InterpFilter
  {
    void (*filterN2_2D)(const ClpRng&, Pel const*, int, Pel*, int, int, int, TFilterCoeff const*, TFilterCoeff const*);
    void (*filterHor[4][2][2])(const ClpRng&, Pel const*, int, Pel*, int, int, int, TFilterCoeff const*);
    void (*filterVer[4][2][2])(const ClpRng&, Pel const*, int, Pel*, int, int, int, TFilterCoeff const*);
    void (*filterCopy[2][2])(const ClpRng&, Pel const*, int, Pel*, int, int, int, bool);
    void (*filter4x4[2][2])(const ClpRng&, Pel const*, int, Pel*, int, int, int, TFilterCoeff const*, TFilterCoeff const*);
    void (*filter8xH[3][2])(const ClpRng&, Pel const*, int, Pel*, int, int, int, TFilterCoeff const*, TFilterCoeff const*);
    void (*filter16xH[3][2])(const ClpRng&, Pel const*, int, Pel*, int, int, int, TFilterCoeff const*, TFilterCoeff const*);
  } interp;

  // ------------------------------------------------------------------
  // 2. RdCost (RdCost.h, ~8.6% of perf)
  // ------------------------------------------------------------------
  struct DistortionTable
  {
    Distortion (*afpDistortFunc[2][VVENC_DF_TOTAL_FUNCTIONS])(const DistParam&);
    void       (*afpDistortFuncX5[2])(const DistParam&, Distortion*, bool);
  } dist;

  // ------------------------------------------------------------------
  // 3. PelBufferOps (Buffer.h, ~1% of perf)
  // ------------------------------------------------------------------
  struct PelBufOps
  {
    void (*addAvg)(const Pel*, const Pel*, Pel*, int, unsigned, int, const ClpRng&);
    void (*reco)(const Pel*, const Pel*, Pel*, int, const ClpRng&);
    void (*copyClip)(const Pel*, Pel*, int, const ClpRng&);
    void (*copyBuffer)(const char*, int, char*, int, int, int);
    void (*roundIntVector)(int*, int, unsigned int, const int);
    uint64_t (*AvgHighPass)(int, int, const Pel*, int);
    uint64_t (*AvgHighPassWithDownsampling)(int, int, const Pel*, int);
  } pelbuf;

  // ------------------------------------------------------------------
  // 4. AdaptiveLoopFilter (AdaptiveLoopFilter.h, ~3.0% of perf)
  // ------------------------------------------------------------------
  struct AlfOps
  {
    void (*deriveClassificationBlk)(AlfClassifier*, const AreaBuf<const Pel>&, const Area&, const Area&, int, int, int);
    void (*filterCcAlf)(const AreaBuf<Pel>&, const UnitBuf<const Pel>&, const Area&, const Area&, const AlfFilterShape&, const short*, const int*);
    void (*filter5x5Blk[2])(const AlfClassifier*, const UnitBuf<Pel>&, const UnitBuf<const Pel>&, const Area&, int);
    void (*filter7x7Blk[2])(const AlfClassifier*, const UnitBuf<Pel>&, const UnitBuf<const Pel>&, const Area&, int);
  } alf;

  // ------------------------------------------------------------------
  // 5. TrQuant + TCoeffOps (TrQuant.h + TrQuant_EMT.h, ~1.6% of perf)
  // ------------------------------------------------------------------
  struct TrQuantOps
  {
    void (*fwdLfnstNxN)(int*, int*, uint32_t, uint32_t, uint32_t, int);
    void (*invLfnstNxN)(int*, int*, uint32_t, uint32_t, uint32_t, int);
    void (*fwdICT)(AreaBuf<Pel>&, AreaBuf<Pel>&);
    void (*invICT)(AreaBuf<Pel>&, AreaBuf<Pel>&);
    void (*fastFwdCore_2D[5])(const TMatrixCoeff*, const TCoeff*, TCoeff*, unsigned, unsigned, unsigned, int);
    void (*fastInvCore[5])(const TMatrixCoeff*, const TCoeff*, TCoeff*, unsigned, unsigned, unsigned);
    void (*roundClip8)(TCoeff*, unsigned, unsigned, unsigned, const TCoeff, const TCoeff, const TCoeff, const TCoeff);
    void (*roundClip4)(TCoeff*, unsigned, unsigned, unsigned, const TCoeff, const TCoeff, const TCoeff, const TCoeff);
  } tr;

  // ------------------------------------------------------------------
  // 6. AffineGradientSearch (AffineGradientSearch.h, ~1.5% of perf)
  // ------------------------------------------------------------------
  struct AffineOps
  {
    void (*horizontalSobelFilter)(Pel*, int, Pel*, int, int, int);
    void (*verticalSobelFilter)(Pel*, int, Pel*, int, int, int);
    void (*equalCoeffComputer[2])(Pel*, int, Pel**, int, int, int, int64_t(*)[7]);
  } affine;

  // ------------------------------------------------------------------
  // 7. IntraPrediction (IntraPrediction.h, ~2.4% of perf)
  // ------------------------------------------------------------------
  struct IntraOps
  {
    void (*angleLuma)(Pel*, ptrdiff_t, Pel*, int, int, int, int, const TFilterCoeff*, bool, const ClpRng&);
    void (*angleChroma)(Pel*, ptrdiff_t, Pel*, int, int, int, int);
    void (*anglePDPC)(Pel*, int, Pel*, int, int, int, int);
    void (*horVerPDPC)(Pel*, int, Pel*, int, int, int, const Pel*, const ClpRng&);
    void (*sampleFilter)(AreaBuf<Pel>&, const AreaBuf<const Pel>&);
    void (*intraPlanar)(AreaBuf<Pel>&, const AreaBuf<const Pel>&);
  } intra;

  // ------------------------------------------------------------------
  // 8. InterPrediction — BDOF/PROF (InterPrediction.h, ~2.0% of perf)
  // ------------------------------------------------------------------
  struct BdofOps
  {
    void (*biDirOptFlow)(const Pel*, const Pel*, const Pel*, const Pel*, const Pel*, const Pel*, int, int, Pel*, ptrdiff_t, int, int, int, const ClpRng&, int);
    void (*bdofGradFilter)(const Pel*, int, int, int, int, Pel*, Pel*, int);
    void (*profGradFilter)(const Pel*, int, int, int, int, Pel*, Pel*, int);
    void (*applyProf)(Pel*, int, const Pel*, int, int, int, const Pel*, const Pel*, int, const int*, const int*, int, const bool&, int, Pel, const ClpRng&);
    void (*padDmvr)(const Pel*, int, Pel*, int, int, int, int);
  } bdof;

  // ------------------------------------------------------------------
  // 9. SampleAdaptiveOffset (SampleAdaptiveOffset.h, <0.5% of perf)
  // ------------------------------------------------------------------
  struct SaoOps
  {
    void (*offsetBlock)(int, int, int, int, Pel*, Pel*, int, int, int64_t*, int64_t*);
    void (*calcEo0)(int, int, int, int, Pel*, Pel*, int, int, int64_t*, int64_t*);
    void (*calcEo90)(int, int, int, int, Pel*, Pel*, int, int, int64_t*, int64_t*, int8_t*);
    void (*calcEo135)(int, int, int, int, Pel*, Pel*, int, int, int64_t*, int64_t*, int8_t*, int8_t*);
    void (*calcEo45)(int, int, int, int, Pel*, Pel*, int, int, int64_t*, int64_t*, int8_t*);
  } sao;

  // ------------------------------------------------------------------
  // 10. MCTF (MCTF.h, <0.5% of perf)
  // ------------------------------------------------------------------
  struct MctfOps
  {
    int (*motionErrorLumaIntX)(const Pel*, ptrdiff_t, const Pel*, ptrdiff_t, int, int, int);
    int (*motionErrorLumaInt8)(const Pel*, ptrdiff_t, const Pel*, ptrdiff_t, int, int, int);
  } mctf;

  // ------------------------------------------------------------------
  // 11. DepQuant / DQInternSimd (DepQuant.h, ~21.7% of perf)
  // ------------------------------------------------------------------
  struct DqOps
  {
    void (*checkAllRdCosts)(DQIntern::ScanPosType, const DQIntern::PQData*, DQIntern::Decisions&, const DQIntern::StateMem&);
    void (*checkAllRdCostsOdd1)(DQIntern::ScanPosType, int64_t, int64_t, DQIntern::Decisions&, const DQIntern::StateMem&);
    void (*updateStates)(const DQIntern::ScanInfo&, const DQIntern::Decisions&, DQIntern::StateMem&);
    void (*updateStatesEOS)(const DQIntern::ScanInfo&, const DQIntern::Decisions&, const DQIntern::StateMem&, DQIntern::StateMem&, DQIntern::CommonCtx&);
    void (*findFirstPos)(int&, const TCoeff*, const DQIntern::TUParameters&, int, bool, int, int);
  } dq;

  // ------------------------------------------------------------------
  // 12. Quant (Quant.h, <0.5% of perf)
  // ------------------------------------------------------------------
  struct QuantOps
  {
    void (*xDeQuant)(int, int, int, const TCoeffSig*, size_t, TCoeff*, int, int, const TCoeff);
    void (*xQuant)(int, int, int, int, int, bool, int);
  } quant;

  // ------------------------------------------------------------------
  // 13. LoopFilter (LoopFilter.h, <0.5% of perf)
  // ------------------------------------------------------------------
  struct LfOps
  {
    void (*pelFilterLuma)(Pel*, ptrdiff_t, ptrdiff_t, int, bool, int, bool, bool, const ClpRng&);
    void (*filterPandQ)(Pel*, ptrdiff_t, ptrdiff_t, int, int, int);
  } lf;
};

extern VVencPrimitive g_vvenc;

void vvenc_setup_primitives(int cpuMask);
void setupCPrimitives(VVencPrimitive& p);
void setupAssemblyPrimitives(VVencPrimitive& p, int cpuMask);
void setupAliasPrimitives(VVencPrimitive& p);

} // namespace vvenc
