/** \file     ExecutorStubs.h
    \brief    Stub executors and SHA-256 verification for bench harness
 */

#pragma once

#include <cstdint>

namespace vvenc {

struct WorkUnit;

class ExecutorStubs
{
public:
    static int registerAll();

    static void sha256(const uint8_t* pData, int size, uint8_t hash[32]);
    static bool hashEqual(const uint8_t a[32], const uint8_t b[32]);

    static bool stubExecutor(WorkUnit* pWu, void* pScratch);

private:
    struct Sha256Ctx
    {
        uint32_t state[8];
        uint64_t count;
        uint8_t  buf[64];
        uint32_t buflen;
    };

    static void xSha256Init(Sha256Ctx* ctx);
    static void xSha256Update(Sha256Ctx* ctx, const uint8_t* data, uint32_t len);
    static void xSha256Final(Sha256Ctx* ctx, uint8_t hash[32]);
    static void xSha256Transform(uint32_t state[8], const uint8_t block[64]);
    static uint32_t xRor(uint32_t x, uint32_t n);
};

}
