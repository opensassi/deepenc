/** \file     PictureDAG.h
    \brief    Wavefront CTU dependency graph builder
 */

#pragma once

#include <cstdint>
#include <atomic>

#include "WorkUnit.h"

namespace vvenc {

class Slice;
class Picture;
struct WorkUnit;

enum CtuWfState : int8_t
{
    WF_NOT_READY    = -1,
    WF_CTU_ENCODE   = 12,
    WF_RECON_WRITE  = 13,
    WF_LF_VER       = 14,
    WF_LF_HOR       = 15,
    WF_SAO_FILTER   = 16,
    WF_ALF_STATS    = 17,
    WF_ALF_DERIVE   = 18,
    WF_ALF_RECON    = 19,
    WF_CCALF_STATS  = 20,
    WF_CCALF_DERIVE = 21,
    WF_CCALF_RECON  = 22,
    WF_DONE         = 23
};

class PictureDAG
{
public:
    static int build(Slice& slice, Picture* pic,
                     WorkUnit* pPool, int poolSize, int& numUnits,
                     std::atomic<int8_t>* pCtuStates);
    static int estimatePoolSize(const Slice& slice);
    static bool checkSpatialDeps(uint32_t ctuRsAddr,
                                  uint16_t ctuPosX, uint16_t ctuPosY,
                                  uint8_t depMask, int8_t requiredStage,
                                  const std::atomic<int8_t>* pCtuStates,
                                  int numCtuCols);

    virtual ~PictureDAG();

private:
    static int xAddCtuEncode(uint32_t rsAddr, uint16_t posX, uint16_t posY,
                             WorkUnit*& pNext, int& numUnits,
                             std::atomic<int8_t>* pCtuStates,
                             int numCtuCols, int numCtuRows);
    static int xAddCtuStage(Stage stage, uint32_t rsAddr,
                            uint16_t posX, uint16_t posY,
                            uint8_t depMask, int8_t requiredStage,
                            WorkUnit* pPrev, WorkUnit*& pNext,
                            int& numUnits,
                            std::atomic<int8_t>* pCtuStates,
                            int numCtuCols);
    static void xLinkStages(WorkUnit* pPrev, WorkUnit* pNext);
    static int8_t xRequiredNeighborStage(Stage stage);
};

}
