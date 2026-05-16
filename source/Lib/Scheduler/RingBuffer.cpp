/** \file     RingBuffer.cpp
    \brief    Lock-free intermediate buffer pool implementation
 */

#include "RingBuffer.h"

#include <cstdlib>
#include <cstring>
#include <cstddef>

namespace vvenc {

int RingBuffer::init(int slotSize, int numSlots)
{
    if (slotSize < 1 || numSlots < 1)
    {
        return -1;
    }

    m_slotSize = slotSize;
    m_numSlots = numSlots;

    m_pData = (uint8_t*)std::malloc((size_t)slotSize * (size_t)numSlots);
    if (!m_pData)
    {
        return -2;
    }

    int words = (numSlots + 63) / 64;
    m_maskWords = words;
    m_pFreeMask = new uint64_t[words]();

    for (int i = 0; i < words; i++)
    {
        uint64_t mask = ~0ULL;
        if (i == words - 1)
        {
            int bitsInLast = numSlots % 64;
            if (bitsInLast > 0)
            {
                mask = (1ULL << bitsInLast) - 1;
            }
        }
        m_pFreeMask[i] = mask;
    }

    m_head.store(0, std::memory_order_relaxed);

    return 0;
}

int RingBuffer::destroy()
{
    if (m_pData)
    {
        std::free(m_pData);
        m_pData = nullptr;
    }
    if (m_pFreeMask)
    {
        delete[] m_pFreeMask;
        m_pFreeMask = nullptr;
    }
    m_slotSize = 0;
    m_numSlots = 0;
    m_maskWords = 0;
    m_head.store(0, std::memory_order_relaxed);
    return 0;
}

RingBuffer::~RingBuffer()
{
    destroy();
}

void* RingBuffer::alloc()
{
    int numSlots = m_numSlots;
    int start = m_head.load(std::memory_order_relaxed);

    for (int i = 0; i < numSlots; i++)
    {
        int idx = (start + i) % numSlots;
        int wordIdx = idx / 64;
        int bitIdx = idx % 64;

        uint64_t word = m_pFreeMask[wordIdx];
        if (word & (1ULL << bitIdx))
        {
            uint64_t expected = word;
            uint64_t desired = word & ~(1ULL << bitIdx);
            if (__atomic_compare_exchange_n(&m_pFreeMask[wordIdx], &expected, desired,
                                            false, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED))
            {
                int newHead = (idx + 1) % numSlots;
                m_head.store(newHead, std::memory_order_relaxed);
                return xIndexToPtr(idx);
            }
        }
    }

    return nullptr;
}

int RingBuffer::free(void* pSlot)
{
    if (!pSlot || !m_pData)
    {
        return -1;
    }

    int idx = xPtrToIndex(pSlot);
    if (idx < 0 || idx >= m_numSlots)
    {
        return -2;
    }

    int wordIdx = idx / 64;
    int bitIdx = idx % 64;

    uint64_t bit = 1ULL << bitIdx;
    __atomic_fetch_or(&m_pFreeMask[wordIdx], bit, __ATOMIC_RELEASE);

    return 0;
}

int RingBuffer::getFreeCount() const
{
    int count = 0;
    for (int i = 0; i < m_maskWords; i++)
    {
        count += __builtin_popcountll(m_pFreeMask[i]);
    }
    return count;
}

int RingBuffer::getCapacity() const
{
    return m_numSlots;
}

int RingBuffer::xPtrToIndex(void* pSlot) const
{
    if (!m_pData)
    {
        return -1;
    }
    ptrdiff_t offset = (uint8_t*)pSlot - m_pData;
    if (offset < 0 || offset >= (ptrdiff_t)m_slotSize * m_numSlots)
    {
        return -1;
    }
    return (int)(offset / m_slotSize);
}

void* RingBuffer::xIndexToPtr(int idx) const
{
    return m_pData + (ptrdiff_t)idx * m_slotSize;
}

}
