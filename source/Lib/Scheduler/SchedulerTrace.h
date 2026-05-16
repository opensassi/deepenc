/** \file     SchedulerTrace.h
    \brief    Binary trace writer for offline scheduler replay
 */

#pragma once

#include <cstdint>
#include <cstdio>
#include <cstddef>

namespace vvenc {

struct WorkUnit;

static constexpr uint32_t SCHED_TRACE_MAGIC = 0x53434854;
static constexpr uint32_t SCHED_TRACE_VERSION = 1;

static constexpr uint8_t RECORD_STAGE       = 0;
static constexpr uint8_t RECORD_OUTPUT      = 1;
static constexpr uint8_t RECORD_FRAME       = 2;
static constexpr uint8_t RECORD_END         = 3;

class SchedulerTrace
{
public:
    int init(const char* pFilename);
    int destroy();

    int recordStage(WorkUnit* pWu, const void* pInputBuf, int inputSize);
    int recordStageRaw(uint32_t tuId, uint8_t stage, uint8_t compId,
                       int8_t qp, uint8_t width, uint8_t height,
                       uint8_t mtsIdx, const void* pInputBuf, int inputSize);
    int recordOutput(WorkUnit* pWu, const void* pOutput, int outSize);
    int recordFrameMarker(uint16_t frameIdx, uint32_t numCtu);
    int recordEnd();

    int getStageCount() const;

    virtual ~SchedulerTrace();

private:
    FILE*   m_pFile     = nullptr;
    int     m_stageCount = 0;
    uint16_t m_frameIdx  = 0;

    int xWriteHeader(uint8_t recordType, uint16_t stageCount, uint32_t payloadSize);
    int xWrite(const void* pData, size_t size);
};

}
