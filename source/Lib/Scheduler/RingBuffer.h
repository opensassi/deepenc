/** \file     RingBuffer.h
    \brief    Lock-free intermediate buffer pool
 */

#pragma once

#include <atomic>
#include <cstdint>

namespace vvenc {

class RingBuffer
{
public:
    int init(int slotSize, int numSlots);
    int destroy();

    void* alloc();
    int free(void* pSlot);

    int getFreeCount() const;
    int getCapacity() const;

    virtual ~RingBuffer();

private:
    uint8_t*    m_pData       = nullptr;
    int         m_slotSize    = 0;
    int         m_numSlots    = 0;

    uint64_t*   m_pFreeMask   = nullptr;
    int         m_maskWords   = 0;

    std::atomic<int> m_head{ 0 };

    int xPtrToIndex(void* pSlot) const;
    void* xIndexToPtr(int idx) const;
};

}
