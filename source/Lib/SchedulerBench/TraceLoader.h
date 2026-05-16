/** \file     TraceLoader.h
    \brief    Binary trace file reader for offline replay
 */

#pragma once

#include <cstdint>
#include <cstdio>
#include <vector>

namespace vvenc {

struct TraceStage
{
    uint32_t tuId;
    uint8_t  stage;
    uint8_t  compId;
    int8_t   qp;
    uint8_t  width, height;
    uint8_t  mtsIdx;
    std::vector<uint8_t> inputData;
    uint8_t  outputHash[32];
};

struct TraceOutput
{
    uint32_t tuId;
    uint8_t  compId;
    uint8_t  finalHash[32];
};

struct TraceFrame
{
    uint16_t frameIdx;
    uint32_t numCtu;
};

class TraceLoader
{
public:
    int load(const char* pFilename);
    int getStageCount() const;
    int getOutputCount() const;
    int getFrameCount() const;
    const TraceStage* getStages() const;
    const TraceOutput* getOutputs() const;
    const TraceFrame* getFrames() const;

private:
    std::vector<TraceStage>  m_stages;
    std::vector<TraceOutput> m_outputs;
    std::vector<TraceFrame>  m_frames;

    int xReadHeader(FILE* pFile);
    int xReadRecord(FILE* pFile);
};

}
