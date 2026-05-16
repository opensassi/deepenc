/** \file     ExecutorStubs.cpp
    \brief    SHA-256 and stub executor implementation
 */

#include "ExecutorStubs.h"
#include "source/Lib/Scheduler/WorkUnit.h"

#include <cstring>

namespace vvenc {

static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

uint32_t ExecutorStubs::xRor(uint32_t x, uint32_t n)
{
    return (x >> n) | (x << (32 - n));
}

void ExecutorStubs::xSha256Transform(uint32_t state[8], const uint8_t block[64])
{
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
    {
        w[i] = ((uint32_t)block[i * 4] << 24)
             | ((uint32_t)block[i * 4 + 1] << 16)
             | ((uint32_t)block[i * 4 + 2] << 8)
             | ((uint32_t)block[i * 4 + 3]);
    }
    for (int i = 16; i < 64; i++)
    {
        uint32_t s0 = xRor(w[i - 15], 7) ^ xRor(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = xRor(w[i - 2], 17) ^ xRor(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t e = state[4], f = state[5], g = state[6], h = state[7];

    for (int i = 0; i < 64; i++)
    {
        uint32_t S1 = xRor(e, 6) ^ xRor(e, 11) ^ xRor(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + S1 + ch + K[i] + w[i];
        uint32_t S0 = xRor(a, 2) ^ xRor(a, 13) ^ xRor(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = S0 + maj;

        h = g; g = f; f = e; e = d + temp1;
        d = c; c = b; b = a; a = temp1 + temp2;
    }

    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

void ExecutorStubs::xSha256Init(Sha256Ctx* ctx)
{
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
    ctx->count = 0;
    ctx->buflen = 0;
}

void ExecutorStubs::xSha256Update(Sha256Ctx* ctx, const uint8_t* data, uint32_t len)
{
    ctx->count += len;

    while (len > 0)
    {
        uint32_t space = 64 - ctx->buflen;
        uint32_t copy = len < space ? len : space;
        std::memcpy(ctx->buf + ctx->buflen, data, copy);
        ctx->buflen += copy;
        data += copy;
        len -= copy;

        if (ctx->buflen == 64)
        {
            xSha256Transform(ctx->state, ctx->buf);
            ctx->buflen = 0;
        }
    }
}

void ExecutorStubs::xSha256Final(Sha256Ctx* ctx, uint8_t hash[32])
{
    uint64_t bits = ctx->count * 8;
    ctx->buf[ctx->buflen++] = 0x80;

    if (ctx->buflen > 56)
    {
        while (ctx->buflen < 64)
        {
            ctx->buf[ctx->buflen++] = 0;
        }
        xSha256Transform(ctx->state, ctx->buf);
        ctx->buflen = 0;
    }

    while (ctx->buflen < 56)
    {
        ctx->buf[ctx->buflen++] = 0;
    }

    for (int i = 0; i < 8; i++)
    {
        ctx->buf[56 + i] = (uint8_t)(bits >> (56 - i * 8));
    }
    xSha256Transform(ctx->state, ctx->buf);

    for (int i = 0; i < 8; i++)
    {
        hash[i * 4]     = (uint8_t)(ctx->state[i] >> 24);
        hash[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 16);
        hash[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 8);
        hash[i * 4 + 3] = (uint8_t)(ctx->state[i]);
    }
}

void ExecutorStubs::sha256(const uint8_t* pData, int size, uint8_t hash[32])
{
    Sha256Ctx ctx;
    xSha256Init(&ctx);
    xSha256Update(&ctx, pData, (uint32_t)size);
    xSha256Final(&ctx, hash);
}

bool ExecutorStubs::hashEqual(const uint8_t a[32], const uint8_t b[32])
{
    for (int i = 0; i < 32; i++)
    {
        if (a[i] != b[i]) return false;
    }
    return true;
}

bool ExecutorStubs::stubExecutor(WorkUnit* pWu, void* pScratch)
{
    if (!pWu) return false;

    int bufSize = pWu->m_width * pWu->m_height * 4;
    if (bufSize < 1) bufSize = 64;

    uint8_t* buf = new uint8_t[bufSize];
    std::memset(buf, 0, bufSize);

    uint8_t hash[32];
    sha256(buf, bufSize, hash);

    delete[] buf;

    return true;
}

int ExecutorStubs::registerAll()
{
    return 0;
}

}
