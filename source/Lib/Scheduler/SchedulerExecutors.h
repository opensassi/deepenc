/** \file     SchedulerExecutors.h
    \brief    Per-stage executor functions for 5-stage TU pipeline
 */

#pragma once

#include <cstdint>

namespace vvenc {

struct WorkUnit;
struct TuStageData;
class IntraSearch;
class InterSearch;
class TempCtx;
class CodingStructure;
class Partitioner;

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
    // ── Per-stage intra executors ──
    static bool execInitPred   (WorkUnit* pWu, void* pScratch);
    static bool execResidual   (WorkUnit* pWu, void* pScratch);
    static bool execFwdXform   (WorkUnit* pWu, void* pScratch);
    static bool execInvXform   (WorkUnit* pWu, void* pScratch);
    static bool execReconstruct(WorkUnit* pWu, void* pScratch);

    // ── Inter executor ──
    static bool execInterTu(WorkUnit* pWu, void* pScratch);
};

}
