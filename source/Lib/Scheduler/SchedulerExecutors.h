/** \file     SchedulerExecutors.h
    \brief    Executor functions wrapping real encoder pipeline stages
 */

#pragma once

#include <cstdint>

namespace vvenc {

struct WorkUnit;
class IntraSearch;

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
};

class SchedulerExecutors
{
public:
    static bool execIntraTu(WorkUnit* pWu, void* pScratch);
    static int  setupIntraTu(IntraSearch* pSearch,
                             WorkUnit* pPool, int numUnits);
};

}
