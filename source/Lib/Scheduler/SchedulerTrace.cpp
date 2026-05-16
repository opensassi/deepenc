/** \file     SchedulerTrace.cpp
    \brief    Binary trace writer implementation
 */

#include "SchedulerTrace.h"
#include "WorkUnit.h"

#include <cstring>

namespace vvenc {

int SchedulerTrace::init(const char* pFilename)
{
    if (!pFilename)
    {
        return -1;
    }

    m_pFile = fopen(pFilename, "wb");
    if (!m_pFile)
    {
        return -2;
    }

    uint32_t header[2];
    header[0] = SCHED_TRACE_MAGIC;
    header[1] = SCHED_TRACE_VERSION;

    if (fwrite(header, sizeof(header), 1, m_pFile) != 1)
    {
        fclose(m_pFile);
        m_pFile = nullptr;
        return -2;
    }

    m_stageCount = 0;
    m_frameIdx = 0;

    return 0;
}

int SchedulerTrace::destroy()
{
    if (m_pFile)
    {
        recordEnd();
        fclose(m_pFile);
        m_pFile = nullptr;
    }
    return 0;
}

SchedulerTrace::~SchedulerTrace()
{
    destroy();
}

int SchedulerTrace::recordStage(WorkUnit* pWu, const void* pInputBuf, int inputSize)
{
    if (!pWu) return -1;
    return recordStageRaw(pWu->m_tuId, (uint8_t)pWu->m_eStage, pWu->m_compId,
                          pWu->m_qp, (uint8_t)pWu->m_width, (uint8_t)pWu->m_height,
                          pWu->m_mtsIdx, pInputBuf, inputSize);
}

int SchedulerTrace::recordStageRaw(uint32_t tuId, uint8_t stage, uint8_t compId,
                                    int8_t qp, uint8_t width, uint8_t height,
                                    uint8_t mtsIdx, const void* pInputBuf, int inputSize)
{
    if (!m_pFile)
    {
        return -1;
    }

    if (inputSize < 0)
    {
        inputSize = 0;
    }
    if (!pInputBuf)
    {
        inputSize = 0;
    }

    uint32_t payloadSize = 4 + 1 + 1 + 1 + 1 + 1 + 1 + 2 + (uint32_t)inputSize + 32;

    int ret = xWriteHeader(RECORD_STAGE, (uint16_t)m_stageCount, payloadSize);
    if (ret < 0) return -2;

    uint16_t inputSize16 = (uint16_t)inputSize;

    if (xWrite(&tuId, 4) < 0) return -2;
    if (xWrite(&stage, 1) < 0) return -2;
    if (xWrite(&compId, 1) < 0) return -2;
    if (xWrite(&qp, 1) < 0) return -2;
    if (xWrite(&width, 1) < 0) return -2;
    if (xWrite(&height, 1) < 0) return -2;
    if (xWrite(&mtsIdx, 1) < 0) return -2;
    if (xWrite(&inputSize16, 2) < 0) return -2;

    if (inputSize16 > 0 && pInputBuf)
    {
        if (xWrite(pInputBuf, inputSize16) < 0) return -2;
    }

    uint8_t zeroHash[32];
    std::memset(zeroHash, 0, 32);
    if (xWrite(zeroHash, 32) < 0) return -2;

    m_stageCount++;
    return 0;
}

int SchedulerTrace::recordOutput(WorkUnit* pWu, const void* pOutput, int outSize)
{
    if (!m_pFile) return -1;
    if (!pWu) return -1;

    uint32_t payloadSize = 4 + 1 + 32;

    int ret = xWriteHeader(RECORD_OUTPUT, 0, payloadSize);
    if (ret < 0) return -2;

    uint32_t tuId = pWu->m_tuId;
    uint8_t compId = (uint8_t)pWu->m_compId;

    if (xWrite(&tuId, 4) < 0) return -2;
    if (xWrite(&compId, 1) < 0) return -2;

    uint8_t zeroHash[32];
    std::memset(zeroHash, 0, 32);
    if (xWrite(zeroHash, 32) < 0) return -2;

    return 0;
}

int SchedulerTrace::recordFrameMarker(uint16_t frameIdx, uint32_t numCtu)
{
    if (!m_pFile) return -1;

    uint32_t payloadSize = 2 + 4;

    int ret = xWriteHeader(RECORD_FRAME, 0, payloadSize);
    if (ret < 0) return -2;

    if (xWrite(&frameIdx, 2) < 0) return -2;
    if (xWrite(&numCtu, 4) < 0) return -2;

    m_frameIdx = frameIdx;
    return 0;
}

int SchedulerTrace::recordEnd()
{
    if (!m_pFile) return 0;

    int ret = xWriteHeader(RECORD_END, 0, 0);
    return ret < 0 ? -2 : 0;
}

int SchedulerTrace::getStageCount() const
{
    return m_stageCount;
}

int SchedulerTrace::xWriteHeader(uint8_t recordType, uint16_t stageCount, uint32_t payloadSize)
{
    uint8_t header[8];
    header[0] = recordType;
    header[1] = 0;
    std::memcpy(header + 2, &stageCount, 2);
    std::memcpy(header + 4, &payloadSize, 4);

    if (fwrite(header, 8, 1, m_pFile) != 1)
    {
        return -1;
    }
    return 0;
}

int SchedulerTrace::xWrite(const void* pData, size_t size)
{
    if (fwrite(pData, size, 1, m_pFile) != 1)
    {
        return -1;
    }
    return 0;
}

}
