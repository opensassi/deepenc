#pragma once

#include <cstdint>
#include <cstddef>
#include <cstdlib>

static constexpr int MAX_TB_SIZEY = 64;
static constexpr int MAX_TB_AREA = MAX_TB_SIZEY * MAX_TB_SIZEY;
static constexpr int NUM_COST_BUF = 8;
static constexpr int NUM_INT_BUF = 3;
static constexpr int NUM_COEFF_BUF = 2;

struct TUParams {
    int width;
    int height;
    int numCoeff;
};

struct AccessContext {
    virtual ~AccessContext() {}
    virtual const char* name() const = 0;
    virtual void simulate(const TUParams& params, int64_t* checksum) = 0;
    virtual size_t totalAllocBytes() const = 0;
};

static inline int64_t mix(int64_t x, int64_t y) {
    return x * 6364136223846793005ULL + y;
}
