#include "access_pattern.h"
#include <cstring>
#include <algorithm>
#include <cassert>

struct Decision {
    int64_t rdCost[4];
    int16_t absLevel[4];
    int8_t  prevId[4];
};

static_assert(sizeof(Decision) >= 44, "Decision must approximate real DepQuant::Decisions size");

static constexpr int SIZE_CLASSES = 5;
static constexpr int SIZE_VALS[SIZE_CLASSES] = { 4, 8, 16, 32, 64 };
static constexpr int SIZE_AREAS[SIZE_CLASSES] = { 16, 64, 256, 1024, 4096 };

struct SubArrayCtx {
    double  (*costBuf)[8]; 
    int     (*intBuf)[3];  
    int64_t (*coeffBuf)[2];
    Decision (*trellis)[2];
};

struct SubArrayContext : AccessContext {
    double  costStorage[SIZE_CLASSES][8][4096];
    int     intStorage[SIZE_CLASSES][3][4096];
    int64_t coeffStorage[SIZE_CLASSES][2][4096];
    Decision trellisStorage[SIZE_CLASSES][4096][2];

    SubArrayContext() {
        for (int s = 0; s < SIZE_CLASSES; s++) {
            int area = SIZE_AREAS[s];
            for (int i = 0; i < 8; i++)
                for (int j = 0; j < area; j++)
                    costStorage[s][i][j] = double(rand() % 100000) / 1000.0;
            for (int i = 0; i < 3; i++)
                for (int j = 0; j < area; j++)
                    intStorage[s][i][j] = rand() % 50000;
            for (int i = 0; i < 2; i++)
                for (int j = 0; j < area; j++)
                    coeffStorage[s][i][j] = int64_t(rand() % 2000 - 1000);
            for (int j = 0; j < area; j++) {
                for (int k = 0; k < 2; k++) {
                    for (int t = 0; t < 4; t++) {
                        trellisStorage[s][j][k].rdCost[t] = rand() % 100000;
                        trellisStorage[s][j][k].absLevel[t] = int16_t(rand() % 256);
                        trellisStorage[s][j][k].prevId[t] = int8_t(rand() % 4);
                    }
                }
            }
        }
    }

    const char* name() const override { return "subarray"; }

    static int sizeClass(int dim) {
        for (int s = 0; s < SIZE_CLASSES; s++)
            if (dim <= SIZE_VALS[s])
                return s;
        return SIZE_CLASSES - 1;
    }

    void simulate(const TUParams& params, int64_t* checksum) override {
        int64_t csum = *checksum;
        int sc = sizeClass(std::max(params.width, params.height));
        int area = SIZE_AREAS[sc];

        double (*cost)[8] = (double(*)[8])&costStorage[sc];
        int (*ibuf)[3] = (int(*)[3])&intStorage[sc];
        int64_t (*coeff)[2] = (int64_t(*)[2])&coeffStorage[sc];
        Decision (*trell)[2] = (Decision(*)[2])&trellisStorage[sc];

        for (int pos = 0; pos < area; pos++) {
            int64_t val = int64_t(pos * 12345 + 6789);
            double errScale = cost[0][pos & 63] * 1.5 + cost[1][pos & 63] * 0.5;

            for (int i = 0; i < 8; i++)
                cost[i][pos] = double(val) * (1.0 + i * 0.1);
            for (int i = 0; i < 3; i++)
                ibuf[i][pos] = int(val % (1000 + i * 500));
            for (int i = 0; i < 2; i++)
                coeff[i][pos] = val / (10 + i * 5);

            for (int k = 0; k < 2; k++) {
                trell[pos][k].rdCost[0] = val + int64_t(errScale);
                trell[pos][k].rdCost[1] = val / 2 + int64_t(errScale);
                trell[pos][k].rdCost[2] = val / 3 + int64_t(errScale);
                trell[pos][k].rdCost[3] = val / 4 + int64_t(errScale);
                trell[pos][k].absLevel[0] = int16_t(val & 0xFF);
                trell[pos][k].prevId[0] = int8_t(pos % 4);
            }

            csum ^= cost[0][pos] > 0 ? int64_t(pos) : int64_t(pos * 2);
        }

        *checksum = csum;
    }

    size_t totalAllocBytes() const override {
        size_t perClass = 8 * MAX_TB_AREA * sizeof(double)
                        + 3 * MAX_TB_AREA * sizeof(int)
                        + 2 * MAX_TB_AREA * sizeof(int64_t)
                        + MAX_TB_AREA * 2 * sizeof(Decision);
        size_t total = 0;
        for (int s = 0; s < SIZE_CLASSES; s++) {
            int area = SIZE_AREAS[s];
            total += 8 * area * sizeof(double)
                   + 3 * area * sizeof(int)
                   + 2 * area * sizeof(int64_t)
                   + area * 2 * sizeof(Decision);
        }
        return total;
    }
};

AccessContext* create_subarray() { return new SubArrayContext(); }
