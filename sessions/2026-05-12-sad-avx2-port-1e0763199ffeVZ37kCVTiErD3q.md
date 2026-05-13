**Session ID:** 2026-05-12-sad-avx2-port

**Date / Duration:** 2026-05-12; prompter active ≈ 3.5 hours

**Project / Context:**
Deepenc (VVenC fork) ASM optimization for the `dist` (RdCost) module. The project replaces C++ SIMD intrinsics with hand-tuned AVX2 assembly for SAD (Sum of Absolute Differences) and partially for HAD (Sum of Absolute Transformed Differences / SATD) kernels used in video encoder rate-distortion optimization.

**Top-Level Component:**
Four SAD AVX2 assembly kernels (8xN, 16xN, 32xN, 64xN) with 5-29% speedup vs C++ SIMD, wired into the encoder dispatch via per-instance function pointer override.

**Second-Level Modules:**
- SAD microbenchmark infrastructure (2M iteration harness with volatile function pointers, perf counters, bit-exact validation)
- SAD8 ASM kernel (+29% speedup, XMM-based, down-counting loop)
- SAD16 ASM kernel (+19% speedup, YMM-based)
- SAD32 ASM kernel (+5% speedup, 2-chunk YMM)
- SAD64 ASM kernel (+18% speedup, 4-chunk YMM, no early-exit)
- Dispatch wiring: `asm-sad_avx2.cpp` + `RdCost.cpp` override call
- C++ pipeline spec for SAD (disassembly analysis, µarch bottleneck model)
- HAD baseline measurement (IPC 2.70, backend bound 50%, 1.16% L1 miss)
- HAD partial ASM implementation (algorithm traced, stage-3 butterfly bug identified)
- Experiment archive at `perf/experiments/sad-avx2-port_2026-05-12/`
- GitHub issue #4 created for HAD completion
- Debugging skill `had-avx2-optimization-4` created with GDB breakpoints and reference values

**Prompter Contributions:**
- Directed the overall two-phase strategy (SAD first, then HAD)
- Chose per-instance dispatch override over full g_vvenc wiring (deferred infrastructure)
- Decided to extend SAD to all 4 sizes before moving to HAD
- Directed creation of experiment archive + experiments index
- Directed creation of GitHub issue + debugging skill for HAD work

**Model Contributions:**
- Built microbenchmark infrastructure for SAD and HAD
- Disassembled and analyzed C++ compiler output for all SAD sizes
- Identified compiler inefficiencies (vmovdqa copy, early-exit branches, loop control)
- Wrote and validated 4 SAD AVX2 assembly kernels
- Wired ASM into encoder dispatch via per-instance table override
- Measured and documented HAD baseline with perf counter analysis
- Identified register spill bottleneck in HAD C++ arrays
- Traced HAD algorithm and verified against C++ reference
- Created experiment archive, experiments index, GitHub issue, and skill

**Prompter Time Estimate:**
- Reading and digesting model responses: ~1.5 hours
- Thinking, strategizing, and weighing options: ~1.0 hours
- Writing messages and directives: ~1.0 hours
- **Total: 3.5 hours**

**Model-Equivalent SME Time Estimate:**
~40 hours of senior performance engineer time:
- Microbenchmark design and implementation: 6 hours
- C++ disassembly analysis and pipeline modeling: 4 hours
- SAD ASM implementation (4 kernels): 12 hours
- Debugging and bit-exact validation: 6 hours
- Dispatch wiring and encoder integration: 4 hours
- HAD baseline measurement and analysis: 4 hours
- Experiment documentation and issue/skill creation: 4 hours

**Required SME Expertise:**
- x86-64 AVX2 assembly programming (Icelake/Sunny Cove µarch)
- Video codec SAD/SSE/Hadmaard transform optimization
- C++ SIMD intrinsic to ASM translation
- GCC inline assembly (.intel_syntax noprefix / .att_syntax)
- System V AMD64 calling convention and register allocation
- perf stat counter analysis (IPC, top-down, cache hierarchy)
- VVenC/VVC encoder dispatch table architecture
- x265 ASM cross-reference navigation (sad16-a.asm, pixel-a.asm)
- GDB debugging of vector register state
- CMake build system and per-file compilation flag management

**Aggregation Tags:**
x86-asm, avx2, sad, hadamard, vvenc, vvc, encoder-optimization, simd, microbenchmark, register-allocation, butterfly, dispatch-table
