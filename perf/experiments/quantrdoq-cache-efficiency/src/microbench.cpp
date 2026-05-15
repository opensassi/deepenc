#include "access_pattern.h"
#include "mode_template.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <chrono>
#include <vector>
#include <string>
#include <algorithm>

extern AccessContext* create_baseline();
extern AccessContext* create_subarray();
extern AccessContext* create_earlyexit();
extern AccessContext* create_template_2x2();
extern AccessContext* create_template_3x3();
extern AccessContext* create_template_4x4();
extern AccessContext* create_template_5x5();
extern AccessContext* create_template_6x6();

struct ModeEntry {
    const char* name;
    AccessContext* (*factory)();
};

static const ModeEntry modes[] = {
    {"baseline",  create_baseline},
    {"subarray",  create_subarray},
    {"earlyexit", create_earlyexit},
    {"t4x4",      create_template_2x2},
    {"t8x8",      create_template_3x3},
    {"t16x16",    create_template_4x4},
    {"t32x32",    create_template_5x5},
    {"t64x64",    create_template_6x6},
};
static constexpr int NUM_MODES = sizeof(modes) / sizeof(modes[0]);

static int parseTuSize(const char* s) {
    int val = atoi(s);
    if (val < 4) val = 4;
    if (val > 64) val = 64;
    return val;
}

static int findMode(const char* name) {
    for (int i = 0; i < NUM_MODES; i++)
        if (strcmp(modes[i].name, name) == 0)
            return i;
    return -1;
}

int main(int argc, char** argv) {
    const char* modeName = "all";
    int tuSize = 16;
    int iterations = 10000;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc)
            modeName = argv[++i];
        else if (strcmp(argv[i], "--tu-size") == 0 && i + 1 < argc)
            tuSize = parseTuSize(argv[++i]);
        else if (strcmp(argv[i], "--iterations") == 0 && i + 1 < argc)
            iterations = atoi(argv[++i]);
        else if (strcmp(argv[i], "--list-modes") == 0) {
            for (int m = 0; m < NUM_MODES; m++)
                printf("%s\n", modes[m].name);
            return 0;
        }
    }

    bool runAll = (strcmp(modeName, "all") == 0);

    fprintf(stderr, "microbench: tu_size=%d iterations=%d mode=%s\n",
            tuSize, iterations, modeName);

    printf("mode,tu_size,tu_area,active_area,iterations,wall_ns,checksum,alloc_bytes\n");

    TUParams params;
    params.width = tuSize;
    params.height = tuSize;
    params.numCoeff = tuSize * tuSize;

    for (int m = 0; m < NUM_MODES; m++) {
        if (!runAll && strcmp(modes[m].name, modeName) != 0)
            continue;

        AccessContext* ctx = modes[m].factory();
        if (!ctx) continue;

        int64_t checksum = 0;
        auto start = std::chrono::high_resolution_clock::now();

        for (int it = 0; it < iterations; it++) {
            ctx->simulate(params, &checksum);
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

        printf("%s,%d,%d,%d,%d,%lu,%ld,%zu\n",
               ctx->name(),
               tuSize,
               tuSize * tuSize,
               tuSize * tuSize,
               iterations,
               (unsigned long)ns,
               (long)checksum,
               ctx->totalAllocBytes());

        delete ctx;
    }

    return 0;
}
