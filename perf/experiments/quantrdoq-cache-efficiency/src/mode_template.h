#pragma once

#include "access_pattern.h"
#include <cstdio>

struct Decision {
    int64_t rdCost[4];
    int16_t absLevel[4];
    int8_t  prevId[4];
};

template<int Log2W, int Log2H>
struct TemplateContext : AccessContext {
    static constexpr int W = 1 << Log2W;
    static constexpr int H = 1 << Log2H;
    static constexpr int AREA = W * H;

    double  costBuf[NUM_COST_BUF][AREA];
    int     intBuf[NUM_INT_BUF][AREA];
    int64_t coeffBuf[NUM_COEFF_BUF][AREA];
    Decision trellis[AREA][2];

    TemplateContext() {
        for (int i = 0; i < NUM_COST_BUF; i++)
            for (int j = 0; j < AREA; j++)
                costBuf[i][j] = double(rand() % 100000) / 1000.0;
        for (int i = 0; i < NUM_INT_BUF; i++)
            for (int j = 0; j < AREA; j++)
                intBuf[i][j] = rand() % 50000;
        for (int i = 0; i < NUM_COEFF_BUF; i++)
            for (int j = 0; j < AREA; j++)
                coeffBuf[i][j] = int64_t(rand() % 2000 - 1000);
        for (int j = 0; j < AREA; j++) {
            for (int k = 0; k < 2; k++) {
                for (int s = 0; s < 4; s++) {
                    trellis[j][k].rdCost[s] = rand() % 100000;
                    trellis[j][k].absLevel[s] = int16_t(rand() % 256);
                    trellis[j][k].prevId[s] = int8_t(rand() % 4);
                }
            }
        }
    }

    const char* name() const override {
        static char buf[32];
        snprintf(buf, sizeof(buf), "template_%dx%d", W, H);
        return buf;
    }

    void simulate(const TUParams& params, int64_t* checksum) override {
        int64_t csum = *checksum;
        int end = AREA;

        for (int pos = 0; pos < end; pos++) {
            int64_t val = int64_t(pos * 12345 + 6789);

            double errScale = costBuf[0][pos & 63] * 1.5 + costBuf[1][pos & 63] * 0.5;

            for (int i = 0; i < NUM_COST_BUF; i++)
                costBuf[i][pos] = double(val) * (0.5 + i * 0.1);
            for (int i = 0; i < NUM_INT_BUF; i++)
                intBuf[i][pos] = int(val % (1000 + i * 500));
            for (int i = 0; i < NUM_COEFF_BUF; i++)
                coeffBuf[i][pos] = val / (10 + i * 5);

            for (int k = 0; k < 2; k++) {
                trellis[pos][k].rdCost[0] = val + int64_t(errScale);
                trellis[pos][k].rdCost[1] = val / 2 + int64_t(errScale);
                trellis[pos][k].rdCost[2] = val / 3 + int64_t(errScale);
                trellis[pos][k].rdCost[3] = val / 4 + int64_t(errScale);
                trellis[pos][k].absLevel[0] = int16_t(val & 0xFF);
                trellis[pos][k].prevId[0] = int8_t(pos % 4);
            }

            csum ^= costBuf[0][pos] > 0 ? int64_t(pos) : int64_t(pos * 2);
        }

        *checksum = csum;
    }

    size_t totalAllocBytes() const override {
        return sizeof(*this);
    }
};
