/** \file     TUPipelineDAG.h
    \brief    TU pipeline dependency graph builder
 */

#pragma once

#include <cstdint>

namespace vvenc {

struct WorkUnit;
struct CodingUnit;

struct MockTU
{
    int      width;
    int      height;
    uint8_t  compMask;
    uint8_t  mtsIdx;
    int8_t   qp;
};

class TUPipelineDAG
{
public:
    static int build(const MockTU* pTus, int numTus,
                     WorkUnit* pPool, int poolSize, int& numUnits);
    static int build(CodingUnit* pCu,
                     WorkUnit* pPool, int poolSize, int& numUnits);
    static int estimatePoolSize(const MockTU* pTus, int numTus);
    static int estimatePoolSize(CodingUnit* pCu);

    virtual ~TUPipelineDAG();

private:
    static int xAddComponentStages(const MockTU& tu, uint32_t tuId,
                                   uint8_t compId,
                                   WorkUnit*& pNext, int& numUnits,
                                   int poolSize,
                                   WorkUnit* pLastInTu);
    static void xLink(WorkUnit* pPrev, WorkUnit* pNext);
};

}
