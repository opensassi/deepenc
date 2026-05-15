/* -----------------------------------------------------------------------------
The copyright in this software is being made available under the Clear BSD
License, included below. No patent rights, trademark rights and/or 
other Intellectual Property Rights other than the copyrights concerning 
the Software are granted under this license.

The Clear BSD License

Copyright (c) 2019-2026, Fraunhofer-Gesellschaft zur Förderung der angewandten Forschung e.V. & The VVenC Authors.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted (subject to the limitations in the disclaimer below) provided that
the following conditions are met:

     * Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.

     * Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.

     * Neither the name of the copyright holder nor the names of its
     contributors may be used to endorse or promote products derived from this
     software without specific prior written permission.

NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY
THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.


------------------------------------------------------------------------------------------- */
/** \file     QuantRDOQ.h
    \brief    RDOQ class (header)
*/

#pragma once

#include "CommonDef.h"
#include "Unit.h"
#include "Contexts.h"
#include "ContextModelling.h"
#include "Quant.h"

//! \ingroup CommonLib
//! \{

namespace vvenc {

// ====================================================================================================================
// Size class helpers — per-size sub-arrays for cache efficiency
// ====================================================================================================================

template<typename T, int A0 = 16, int A1 = 64, int A2 = 256, int A3 = 1024, int A4 = 4096>
struct SizedBuf
{
  T m_data[A0 + A1 + A2 + A3 + A4];

  T* ptr(int scIdx) const
  {
    static const int offs[5] = { 0, A0, A0 + A1, A0 + A1 + A2, A0 + A1 + A2 + A3 };
    return const_cast<T*>(m_data + offs[scIdx]);
  }
  int area(int scIdx) const
  {
    static const int areas[5] = { A0, A1, A2, A3, A4 };
    return areas[scIdx];
  }
};

template<typename T, int N2 = 1>
struct SizedBuf2
{
  T m_data[(16 + 64 + 256 + 1024 + 4096) * N2];

  T* ptr(int scIdx) const
  {
    static const int offs[5] = { 0, 16 * N2, (16 + 64) * N2, (16 + 64 + 256) * N2, (16 + 64 + 256 + 1024) * N2 };
    return const_cast<T*>(m_data + offs[scIdx]);
  }
  int area(int scIdx) const
  {
    static const int areas[5] = { 16 * N2, 64 * N2, 256 * N2, 1024 * N2, 4096 * N2 };
    return areas[scIdx];
  }
};

struct SizeClass
{
  static int idx(int dim)
  {
    return dim <= 4 ? 0 : dim <= 8 ? 1 : dim <= 16 ? 2 : dim <= 32 ? 3 : 4;
  }
  static int area(int scIdx)
  {
    static const int sAreas[5] = { 16, 64, 256, 1024, 4096 };
    return sAreas[scIdx];
  }
};

// ====================================================================================================================
// Class definition
// ====================================================================================================================

/// transform and quantization class
class QuantRDOQ : public Quant
{
public:
  QuantRDOQ( const Quant* other, bool useScalingLists );
  ~QuantRDOQ();

public:
  void setFlatScalingList   ( const int maxLog2TrDynamicRange[MAX_NUM_CH], const BitDepths &bitDepths );
  // quantization
  void quant                ( TransformUnit& tu, const ComponentID compID, const CCoeffBuf& pSrc, TCoeff &uiAbsSum, const QpParam& cQP, const Ctx& ctx );
  void forwardRDPCM         ( TransformUnit& tu, const ComponentID compID, const CCoeffBuf& pSrc, TCoeff &uiAbsSum, const QpParam& cQP, const Ctx &ctx );
  void rateDistOptQuantTS   ( TransformUnit& tu, const ComponentID compID, const CCoeffBuf& coeffs, TCoeff &absSum, const QpParam& qp, const Ctx &ctx );

private:
  double* xGetErrScaleCoeffSL            ( uint32_t list, uint32_t sizeX, uint32_t sizeY, int qp ) { return m_errScale[sizeX][sizeY][list][qp]; };  //!< get Error Scale Coefficent
  double  xGetErrScaleCoeff              ( const bool needsSqrt2, SizeType width, SizeType height, int qp, const int maxLog2TrDynamicRange, const int channelBitDepth, bool bTransformSkip);
  double& xGetErrScaleCoeffNoScalingList ( uint32_t list, uint32_t sizeX, uint32_t sizeY, int qp ) { return m_errScaleNoScalingList[sizeX][sizeY][list][qp]; };  //!< get Error Scale Coefficent
  void    xInitScalingList               ( const QuantRDOQ* other );
  void    xDestroyScalingList            ();
  void    xSetErrScaleCoeff              ( uint32_t list, uint32_t sizeX, uint32_t sizeY, int qp, const int maxLog2TrDynamicRange[MAX_NUM_CH], const BitDepths &bitDepths );
  void    xDequantSample                 ( TCoeff& pRes, TCoeffSig& coeff, const TrQuantParams& trQuantParams );
  // RDOQ functions
  void    xRateDistOptQuant              ( TransformUnit& tu, const ComponentID compID, const CCoeffBuf& pSrc, TCoeff &uiAbsSum, const QpParam& cQP, const Ctx &ctx);

  inline uint32_t xGetCodedLevel( double&            rd64CodedCost,
                                  double&            rd64CodedCost0,
                                  double&            rd64CodedCostSig,
                                  Intermediate_Int   lLevelDouble,
                                  uint32_t           uiMaxAbsLevel,
                                  const BinFracBits* fracBitsSig,
                                  const BinFracBits& fracBitsPar,
                                  const BinFracBits& fracBitsGt1,
                                  const BinFracBits& fracBitsGt2,
                                  const int          remRegBins,
                                  unsigned           goRiceZero,
                                  uint16_t           ui16AbsGoRice,
                                  int                iQBits,
                                  double             errorScale,
                                  bool               bLast,
                                  const int          maxLog2TrDynamicRange ) const;
  inline int xGetICRate     ( const uint32_t         uiAbsLevel,
                              const BinFracBits& fracBitsPar,
                              const BinFracBits& fracBitsGt1,
                              const BinFracBits& fracBitsGt2,
                              const int          remRegBins,
                              unsigned           goRiceZero,
                              const uint16_t       ui16AbsGoRice,
                              const int          maxLog2TrDynamicRange  ) const;
  inline double xGetRateLast         ( const int* lastBitsX, const int* lastBitsY,
                                       unsigned        PosX, unsigned   PosY                              ) const;

  inline double xGetRateSigCoeffGroup( const BinFracBits& fracBitsSigCG,   unsigned uiSignificanceCoeffGroup ) const;

  inline double xGetRateSigCoef      ( const BinFracBits& fracBitsSig,     unsigned uiSignificance           ) const;

  inline double xGetICost            ( double dRate                                                      ) const;
  inline double xGetIEPRate          (                                                                   ) const;

  inline uint32_t xGetCodedLevelTSPred( double&             rd64CodedCost,
                                        double&             rd64CodedCost0,
                                        double&             rd64CodedCostSig,
                                        Intermediate_Int    levelDouble,
                                        int                 qBits,
                                        double              errorScale,
                                        uint32_t            coeffLevels[],
                                        double              coeffLevelError[],
                                        const BinFracBits*  fracBitsSig,
                                        const BinFracBits&  fracBitsPar,
                                        CoeffCodingContext& cctx,
                                        const FracBitsAccess& fracBitsAccess,
                                        const BinFracBits&  fracBitsSign,
                                        const BinFracBits&  fracBitsGt1,
                                        const uint8_t       sign,
                                        int                 rightPixel,
                                        int                 belowPixel,
                                        uint16_t            ricePar,
                                        bool                isLast,
                                        const int           maxLog2TrDynamicRange,
                                        int&                numUsedCtxBins
                                      ) const;

  inline int xGetICRateTS   ( const uint32_t            absLevel,
                              const BinFracBits&        fracBitsPar,
                              const CoeffCodingContext& cctx,
                              const FracBitsAccess&     fracBitsAccess,
                              const BinFracBits&        fracBitsSign,
                              const BinFracBits&        fracBitsGt1,
                              int&                      numCtxBins,
                              const uint8_t             sign,
                              const uint16_t            ricePar,
                              const int                 maxLog2TrDynamicRange  ) const;
private:
  bool    m_isErrScaleListOwner;

  double* m_errScale              [SCALING_LIST_SIZE_NUM][SCALING_LIST_SIZE_NUM][SCALING_LIST_NUM][SCALING_LIST_REM_NUM]; ///< array of quantization matrix coefficient 4x4
  double  m_errScaleNoScalingList [SCALING_LIST_SIZE_NUM][SCALING_LIST_SIZE_NUM][SCALING_LIST_NUM][SCALING_LIST_REM_NUM]; ///< array of quantization matrix coefficient 4x4
  // temporary buffers for RDOQ — per-size sub-arrays for cache efficiency
  SizedBuf<double>                m_pdCostCoeff;
  SizedBuf<double>                m_pdCostSig;
  SizedBuf<double>                m_pdCostCoeff0;
  SizedBuf<double,1,4,16,64,256> m_pdCostCoeffGroupSig;
  SizedBuf<int>                   m_rateIncUp;
  SizedBuf<int>                   m_rateIncDown;
  SizedBuf<int>                   m_sigRateDelta;
  SizedBuf<TCoeff>                m_deltaU;
  SizedBuf<TCoeff>                m_fullCoeff;
  int     m_bdpcm;
  int     m_testedLevels;
};// END CLASS DEFINITION QuantRDOQ

} // namespace vvenc

//! \}

