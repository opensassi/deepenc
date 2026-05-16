/** \file     TraceLoader.cpp
    \brief    Binary trace file reader implementation
 */

#include "TraceLoader.h"

#include "source/Lib/Scheduler/SchedulerTrace.h"

#include <cstring>

namespace vvenc {

int TraceLoader::load(const char* pFilename)
{
    if (!pFilename)
    {
        return -1;
    }

    FILE* pFile = fopen(pFilename, "rb");
    if (!pFile)
    {
        return -2;
    }

    int ret = xReadHeader(pFile);
    if (ret < 0)
    {
        fclose(pFile);
        return ret;
    }

    m_stages.clear();
    m_outputs.clear();
    m_frames.clear();

    while (true)
    {
        ret = xReadRecord(pFile);
        if (ret < 0)
        {
            break;
        }
        if (ret == 1)
        {
            break;
        }
    }

    fclose(pFile);
    return 0;
}

int TraceLoader::xReadHeader(FILE* pFile)
{
    uint32_t header[2];
    if (fread(header, sizeof(header), 1, pFile) != 1)
    {
        return -3;
    }

    if (header[0] != SCHED_TRACE_MAGIC)
    {
        return -4;
    }
    if (header[1] != SCHED_TRACE_VERSION)
    {
        return -5;
    }

    return 0;
}

int TraceLoader::xReadRecord(FILE* pFile)
{
    uint8_t recordHeader[8];
    if (fread(recordHeader, 8, 1, pFile) != 1)
    {
        return -1;
    }

    uint8_t  recordType = recordHeader[0];
    uint16_t stageCount;
    uint32_t payloadSize;
    std::memcpy(&stageCount, recordHeader + 2, 2);
    std::memcpy(&payloadSize, recordHeader + 4, 4);

    if (payloadSize > 1024 * 1024)
    {
        return -6;
    }

    switch (recordType)
    {
        case RECORD_STAGE:
        {
            TraceStage ts;

            uint8_t stage, compId;
            int8_t qp;
            uint8_t width, height, mtsIdx;
            uint16_t inputSize;

            if (fread(&ts.tuId, 4, 1, pFile) != 1) return -2;
            if (fread(&stage, 1, 1, pFile) != 1) return -2;
            if (fread(&compId, 1, 1, pFile) != 1) return -2;
            if (fread(&qp, 1, 1, pFile) != 1) return -2;
            if (fread(&width, 1, 1, pFile) != 1) return -2;
            if (fread(&height, 1, 1, pFile) != 1) return -2;
            if (fread(&mtsIdx, 1, 1, pFile) != 1) return -2;
            if (fread(&inputSize, 2, 1, pFile) != 1) return -2;

            ts.stage = stage;
            ts.compId = compId;
            ts.qp = qp;
            ts.width = width;
            ts.height = height;
            ts.mtsIdx = mtsIdx;

            if (inputSize > 0)
            {
                ts.inputData.resize(inputSize);
                if (fread(ts.inputData.data(), inputSize, 1, pFile) != 1)
                {
                    return -2;
                }
            }

            if (fread(ts.outputHash, 32, 1, pFile) != 1)
            {
                return -2;
            }

            m_stages.push_back(ts);
            break;
        }

        case RECORD_OUTPUT:
        {
            TraceOutput to;
            uint8_t compId;

            if (fread(&to.tuId, 4, 1, pFile) != 1) return -2;
            if (fread(&compId, 1, 1, pFile) != 1) return -2;
            if (fread(to.finalHash, 32, 1, pFile) != 1) return -2;

            to.compId = compId;
            m_outputs.push_back(to);
            break;
        }

        case RECORD_FRAME:
        {
            TraceFrame tf;

            if (fread(&tf.frameIdx, 2, 1, pFile) != 1) return -2;
            if (fread(&tf.numCtu, 4, 1, pFile) != 1) return -2;

            m_frames.push_back(tf);
            break;
        }

        case RECORD_END:
            return 1;

        default:
            if (payloadSize > 0)
            {
                fseek(pFile, payloadSize, SEEK_CUR);
            }
            break;
    }

    return 0;
}

int TraceLoader::getStageCount() const { return (int)m_stages.size(); }
int TraceLoader::getOutputCount() const { return (int)m_outputs.size(); }
int TraceLoader::getFrameCount() const { return (int)m_frames.size(); }
const TraceStage* TraceLoader::getStages() const { return m_stages.data(); }
const TraceOutput* TraceLoader::getOutputs() const { return m_outputs.data(); }
const TraceFrame* TraceLoader::getFrames() const { return m_frames.data(); }

}
