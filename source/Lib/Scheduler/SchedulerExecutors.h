/** \file     SchedulerExecutors.h
    \brief    Executor functions wrapping real encoder pipeline stages
 */

#pragma once

#include <cstdint>

namespace vvenc {

struct WorkUnit;
class IntraSearch;
class InterSearch;
class TempCtx;
class CodingStructure;
class Partitioner;

struct IntraTuExecCtx
{
    IntraSearch*     pSearch;
    void*            pTu;
    uint8_t          compId;
    bool             checkCrossCPrediction;
    uint64_t*        pDist;
    uint32_t*        pNumSig;
    void*            pPred;
    bool             loadTr;
    TempCtx*         pCtxStart;
};

struct InterTuExecCtx
{
    InterSearch*     pSearch;
    CodingStructure* pCs;
    Partitioner*     pPartitioner;
    uint64_t*        pZeroDist;
    TempCtx*         pCtxStart;
};

class SchedulerExecutors
{
public:
    static bool execIntraTu(WorkUnit* pWu, void* pScratch);
    static int  setupIntraTu(IntraSearch* pSearch,
                             WorkUnit* pPool, int numUnits);
    static bool execInterTu(WorkUnit* pWu, void* pScratch);
};

}
