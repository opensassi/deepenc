#include "access_pattern.h"
#include <cstring>
#include <algorithm>

struct Decision {
    int64_t rdCost[4];
    int16_t absLevel[4];
    int8_t  prevId[4];
};

static_assert(sizeof(Decision) >= 44, "Decision must approximate real DepQuant::Decisions size");

struct BaselineContext : AccessContext {
    double  costBuf[NUM_COST_BUF][MAX_TB_AREA];
    int     intBuf[NUM_INT_BUF][MAX_TB_AREA];
    int64_t coeffBuf[NUM_COEFF_BUF][MAX_TB_AREA];
    Decision trellis[MAX_TB_AREA][2];

    BaselineContext() {
        for (int i = 0; i < NUM_COST_BUF; i++)
            for (int j = 0; j < MAX_TB_AREA; j++)
                costBuf[i][j] = double(rand() % 100000) / 1000.0;
        for (int i = 0; i < NUM_INT_BUF; i++)
            for (int j = 0; j < MAX_TB_AREA; j++)
                intBuf[i][j] = rand() % 50000;
        for (int i = 0; i < NUM_COEFF_BUF; i++)
            for (int j = 0; j < MAX_TB_AREA; j++)
                coeffBuf[i][j] = int64_t(rand() % 2000 - 1000);
        for (int j = 0; j < MAX_TB_AREA; j++) {
            for (int k = 0; k < 2; k++) {
                for (int s = 0; s < 4; s++) {
                    trellis[j][k].rdCost[s] = rand() % 100000;
                    trellis[j][k].absLevel[s] = int16_t(rand() % 256);
                    trellis[j][k].prevId[s] = int8_t(rand() % 4);
                }
            }
        }
    }

    const char* name() const override { return "baseline"; }

    void simulate(const TUParams& params, int64_t* checksum) override {
        int64_t csum = *checksum;
        int end = MAX_TB_AREA;

        for (int pos = 0; pos < end; pos++) {
            int64_t val = int64_t(pos * 12345 + 6789);

            double errScale = costBuf[0][pos & 63] * 1.5 + costBuf[1][pos & 63] * 0.5;

            costBuf[0][pos] = double(val) * errScale;
            costBuf[1][pos] = double(val) * 0.5;
            costBuf[2][pos] = double(val) * 1.2;
            costBuf[3][pos] = double(val) * 0.8;
            costBuf[4][pos] = double(val) * 1.1;
            costBuf[5][pos] = double(val) * 0.9;
            costBuf[6][pos] = double(val) * 1.4;
            costBuf[7][pos] = double(val) * 0.7;

            intBuf[0][pos] = int(val % 1000);
            intBuf[1][pos] = int(val % 500);
            intBuf[2][pos] = int(val % 2000);

            coeffBuf[0][pos] = val / 10;
            coeffBuf[1][pos] = val / 100;

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
        return NUM_COST_BUF * MAX_TB_AREA * sizeof(double)
             + NUM_INT_BUF * MAX_TB_AREA * sizeof(int)
             + NUM_COEFF_BUF * MAX_TB_AREA * sizeof(int64_t)
             + MAX_TB_AREA * 2 * sizeof(Decision);
    }
};

AccessContext* create_baseline() { return new BaselineContext(); }
