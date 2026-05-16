---
name: asm-optimizer
description: Evaluate and optimize VVenC hot functions through assembly translation — perf-based baseline profiling, x265 cross-reference, microbenchmark validation, and iterative improvement
---

# Skill: asm-optimizer

## Persona

Senior performance engineer with deep expertise in x86 assembly optimization, microarchitecture analysis (frontend/backend bound, cache hierarchy, branch prediction, load/store queues), and video codec SIMD kernels.

## On Activation

1. Read the dispatch table from `source/Lib/CommonLib/Primitives.spec.md §3 Dispatch Table Catalog`
2. Check for existing baseline profiles in `perf/baseline/profiles/`
3. Load x265 ASM cross-reference from `external/x265-reference.md`
4. Present a sorted priority list of functions ranked by optimization potential
5. Show available commands

## Dependencies

- `source/Lib/CommonLib/Primitives.spec.md` — dispatch table catalog
- `source/Lib/CommonLib/Primitives.h` — VVencPrimitive struct
- `.profiler/perf_archives/` — previous profiling data
- `external/x265-reference.md` — cross-reference mapping
- `external/x265/source/common/x86/*.spec.md` — x265 ASM reference implementations
- `perf/baseline/` — baseline build and profiles (generated, gitignored)
- `scripts/asm-optimizer/` — support scripts
- `scripts/extract-artifacts.js` — artifact extraction for technical specifications
- `scripts/test-artifacts.js` — artifact validation (mermaid→PNG, D3 filmstrip)
- `scripts/verify-animation.js` — D3 animation keyframe verification
- `scripts/install/` — platform install scripts (setup `nasm`, toolchain)

## Commands

### `setup-baseline`

Create the baseline directory structure, clone vvenc-v1.14.0, build the Release encoder, and run the full profiling matrix (fast/slow presets × 5/50 frame lengths) using `scripts/asm-optimizer/run-baseline.sh`.

Output:
```
perf/baseline/
├── vvenc-1.14.0/              ← release tag checkout
├── profiles/default/
│   ├── fast-5fr/
│   ├── fast-50fr/
│   ├── slow-5fr/
│   └── slow-50fr/
└── reports/profile-summary.json
```

### `profile <name> [--preset PRESET] [--frames N]`

Run a maximal perf counter dump against the baseline encoder. All counters listed below are recorded. Saves to `perf/baseline/profiles/<name>/`.

Default: `--preset fast --frames 5`

If `--preset vvenc` is used, profiles the **current working tree** encoder (not baseline) for comparison.

### `assess <entry>`

Evaluate one dispatch table entry for ASM optimization potential.

Reads from:
- The dispatch table catalog in `Primitives.spec.md §3`
- The baseline profile matching closest preset/frame config
- The x265 reference implementation if one exists

Reports:
- Current C++ intrinsic implementation
- Perf counter analysis (IPC, cache misses, branch mispredicts)
- Memory vs compute bound classification
- x265 equivalent with link to its `.spec.md`
- Estimated speedup potential (Low / Medium / High / Critical)
- Recommendation (port x265, write from scratch, skip)

### `assess all`

Run assessment on every entry in the dispatch table. Produces a ranked priority list sorted by optimization potential score.

### `setup-microbench <entry>`

Create an isolated microbenchmark for one dispatch table function. Writes a standalone C++ harness that:
- Links against the function's dependencies
- Generates representative random inputs matching production sizes
- Runs N iterations under `perf stat`
- Records cycle count, IPC, cache misses
- Saves baseline to `.profiler/asm-optimizer/baselines/<entry>`

### `spec <entry>`

Generate a technical specification of the C++ reference implementation using the system-design approach:

1. **Disassemble the C++ SIMD function**: Use `objdump -d` on the compiled binary to extract the C++ compiler's output. Save as `source/Lib/CommonLib/x86/<entry>-cpp-spec.spec.md`.

2. **Count instructions**: Use `grep -c "^    [0-9a-f]"` on the disassembly to get the full instruction count. Break down by functional blocks.

3. **Build a pipeline model**: Identify key µarch features:
   - Frontend (decode width = 4-wide)
   - Execution ports (P0-P6 for Sunny Cove)
   - Memory hierarchy (L1D 48KB, LDQ 12 entries)
   - Cache working set analysis

4. **Create a technical specification** with:
   - Architecture diagram (Mermaid C4 graph of pipeline components)
   - Sequence diagram (instruction flow through pipeline stages)
   - D3 animation for cycle-level visualization
   - Bottleneck analysis table
   - Instruction-to-uop decomposition table

5. **Use the artifact pipeline** to validate extracted diagrams:
   ```
   node scripts/extract-artifacts.js --file <spec-path>
   node scripts/test-artifacts.js --file <spec-path>
   ```

6. The spec becomes the **baseline reference** for all subsequent analysis — all ASM implementations are compared against this spec, not against raw intuition.

### `analyze-gap <entry>`

Compare the ASM implementation against the C++ spec baseline:

1. **Disassemble both**:
   ```
   objdump -d <microbench> | awk '/<vvenc_...>/,/^$/' | grep -c "^    [0-9a-f]"
   objdump -d <microbench> | awk '/<DQInternSimd.*>/,/^$/' | grep -c "^    [0-9a-f]"
   ```

2. **Compare instruction count per functional block**: rdCost setup, sigBits, cffBits, spt dispatch, min-select, store/epilogue.

3. **Identify structural differences**, prioritized by estimated impact:

   **Critical (5+ uops, 3+ cycles)**:
   - Loop structure / block count vs C++ reference
   - Different algorithm or data flow

   **High (3-5 uops, 1-3 cycles)**:
   - Is the compiler using LEA chains instead of IMUL? (e.g., `ctx*3` then `*8` = ×24)
   - Are memory-folded vpinsrd/vpinsrq used instead of separate `mov`+`vpinsrd`?
   - Are address computations precomputed or re-computed per load?

   **Medium (1-3 uops, <1 cycle)**:
   - Is register scheduling different?
   - Excess register spills (temporary stores to stack)

   **Low (cosmetic only)**:
   - Instruction selection differences (e.g., vpaddd vs vpaddw)

4. **Rate each gap** on potential impact:
   - **Critical**: 5+ uops saved, 3+ cycles
   - **High**: 3-5 uops saved, 1-3 cycles
   - **Medium**: 1-3 uops saved, <1 cycle
   - **Low**: cosmetic only

5. **Output** a structured gap analysis: for each gap, the C++ approach, our ASM approach, the estimated uop/cycle difference, and a fix recommendation.

### `bench <entry>`

Run the microbenchmark and compare against the C++ SIMD baseline:

1. Build the microbenchmark with `-fno-inline` to prevent the C++ function from being inlined into the benchmark loop.
2. Call both functions through **volatile function pointers** to force indirect calls (no inlining advantage).
3. Record:
   - C++ SIMD ref time
   - ASM time
   - Speedup ratio (ASM / C++ = 1.0 means equal)
4. Report whether the result is above the significance threshold (see Benchmark Environment notes).

### `implement <entry> [--ref x265-asm-path]`

Generate an implementation for one dispatch table entry, following the spec-first process:

1. **Generate spec first**: If no spec exists for this entry (from `spec <entry>`), tell the user to run `spec <entry>` first and abort.
2. **Analyze the gap**: If no gap analysis exists, run `analyze-gap <entry>` to identify which structural improvements to target.
3. **Propose a hypothesis**: For each identified gap, propose a specific ASM change. Create a mini-spec for the hypothesis explaining what it changes and why.
4. **Write the ASM**: Write a NASM `.asm` file in `source/Lib/CommonLib/x86/`. Only use GAS inline asm in `.cpp` if NASM is unavailable. **NASM caveat**: All YMM instructions using ymm0–ymm7 require `{vex3}` prefix — without it, NASM silently emits VEX 2-byte (128-bit) encoding, zeroing the upper 128 bits. Verify with `objdump -d` (look for `c4` prefix = 256-bit, `c5` = 128-bit).
5. **Register**: Add the `extern "C"` declaration in `asm-primitives.h` and the function pointer assignment in `asm-primitives.cpp::setupAssemblyPrimitives()`. NASM `.asm` files in `x86/` are auto-detected by CMake via glob — no manual CMake edits needed.
6. **Validate struct offsets**: Before relying on struct field accesses in ASM code:
   - Read the struct definition (header file), tracing through any base class inheritance
   - Account for alignment and padding between fields
   - Cross-validate each offset against any existing working ASM function that accesses the same fields
   - When in doubt, verify with `offsetof(struct, field)` in a compile-time assertion or `(uintptr_t)&((struct*)0)->field`
8. **Validate bit-exactness**: Run bit-exact test (all 16+ test patterns must pass).
9. **Benchmark**: Run `bench <entry>` against the C++ SIMD baseline.
10. **Evaluate**: If the improvement is above the significance threshold, accept. If below, archive as experiment.

### `iterative-optimize <entry> [--iter N]`

Full optimization pipeline with experiment archiving:

1. `setup-microbench <entry>` — create/update harness
2. `spec <entry>` — generate C++ technical specification
3. For each hypothesis (up to N iterations):
   a. `analyze-gap <entry>` — compare to C++ baseline
   b. `implement <entry>` — try one improvement
   c. `bench <entry>` — measure against C++ SIMD
   d. If speedup >= threshold: accept, commit, continue
   e. If speedup < threshold: archive experiment

4. **If after N iterations no hypothesis achieves significant improvement**:
   - Run `archive-experiment <entry>` with the final results
   - Only the experiment files are committed (not the code changes)
   - Working tree changes remain uncommitted for other agents

5. Report final outcome: which improvements succeeded, which were archived, and the per-hypothesis benchmark table.

### `archive-experiment <entry>`

Save a complete experiment record when a hypothesis does not yield significant improvement:

1. Create `perf/experiments/<entry>_<date>/` with:
   - `src/` — microbenchmark, ASM source, build script
   - `specs/` — technical specifications generated during analysis
   - `results/` — benchmark data, perf stat output, comparison tables
   - `README.md` — session summary, hypothesis tried, benchmark results, conclusions

2. Stage only the experiment directory: `git add perf/experiments/<entry>_<date>/`
3. Do NOT revert other working tree changes.
4. Report the experiment path and a summary.

### `report [--format markdown|json]`

Generate an optimization report covering all assessed/optimized entries with measured speedups, x265 comparison, and recommendations.

## Assessment Methodology

Each dispatch table entry is scored against these factors:

| Factor | Source | Weight |
|--------|--------|--------|
| Perf share (% samples) | Baseline profile flamegraph | Primary sort key |
| IPC of current impl | `perf stat` on microbench | < 1.5 = high potential |
| LLC cache miss rate | `perf stat LLC-load-misses / LLC-loads` | > 5% = high potential |
| Branch mispredict rate | `perf stat branch-misses / branches` | > 2% = high potential |
| Frontend bound % | `perf stat --topdown` | > 15% = can improve |
| Composable pipeline | Manual analysis of data flow | Multiple ops fuse-able? |
| Compiler gap | Instruction count diff from C++ baseline | > 20% more instr = high potential |
| x265 ASM reference | x265 spec.md tree | Direct port possible? |
| Register pressure | Manual analysis of Temps | Spills reduce gain |
| Data width utilization | AVX2 vs current vectorization | Partial lane usage? |

Score → **Low / Medium / High / Critical**

### Benchmark Environment

| Factor | Workstation | Laptop |
|--------|------------|--------|
| Turbo boost | Disable for reproducibility | Keep enabled (no control) |
| Significance threshold | ~5% speedup | ~15-20% speedup |
| Runs per measurement | 3-5 | 5-10 |
| Suggested approach | microbench + full encoder | microbench-only (encoder noise too high) |

On a laptop, **microbenchmark-only measurements** are recommended. Full encoder
wall-clock comparisons are high-noise and should not be used to determine significance.
The significance threshold should account for:
- CPU frequency scaling (turbo boost, thermal throttling)
- Background processes (GUI, browser, etc.)
- Shared memory bandwidth with integrated GPU
- `taskset -c N` should be used for all measurements

### Experiment Archiving

When an optimization hypothesis does not achieve the significance threshold:

1. The experiment is saved to `perf/experiments/<entry>_<date>/`
2. All artifacts (ASM source, benchmark data, pipeline specs) are included
3. The experiment directory is `git add`-ed but NOT committed (session workflow handles commit)
4. The working tree changes (ASM code, registration changes) are **preserved** — not reverted
5. This ensures the session's work is archived even when it doesn't produce a winning optimization

## Baseline Profile Counter Set

Maximal capture — we don't filter yet, we capture everything:

```
cycles,instructions,branches,branch-misses,
cache-references,cache-misses,
L1-dcache-loads,L1-dcache-load-misses,L1-dcache-stores,
L1-icache-loads,L1-icache-load-misses,
LLC-loads,LLC-load-misses,LLC-stores,LLC-store-misses,
dTLB-loads,dTLB-load-misses,dTLB-stores,dTLB-store-misses,
iTLB-loads,iTLB-load-misses,
node-loads,node-load-misses,node-stores,node-store-misses,
alignment-faults,
context-switches,cpu-migrations,page-faults,
stalled-cycles-frontend,stalled-cycles-backend,
fp_arith_inst_retired.256b_packed_single,
fp_arith_inst_retired.128b_packed_single,
fp_arith_inst_retired.scalar_single,
mem_load_uops_retired.l1_hit,mem_load_uops_retired.l1_miss,
mem_load_uops_retired.l2_hit,mem_load_uops_retired.l2_miss,
mem_load_uops_retired.llc_hit,mem_load_uops_retired.llc_miss
```

## Debugging ASM Implementations

When a new ASM implementation crashes, produces wrong results, or is slower than the C++ baseline, follow this methodology:

### Hypothesis-Driven Debugging

**Trigger**: Any test failure or unexpected runtime error.

1. **Do not modify any code.** Instead, open a sub-agent or a structured reasoning block dedicated to diagnosis.

2. **Formulate exactly 3 hypotheses** about the root cause of the failure. Each hypothesis must be a specific, falsifiable statement (e.g., "The YMM register contains the wrong data after the `vpgatherdd` because the index vector was not zero‑extended"). If the problem is clearly more complex, you may generate up to 5 hypotheses, but 3 is the default.

3. **For each hypothesis, write a 1–3 step debugger plan** that will **conclusively confirm or reject** that hypothesis. A good plan:
   - Specifies the exact breakpoint location (function, file:line, or symbol).
   - Identifies the critical variables, registers, or memory regions to inspect.
   - Describes the expected value *if the hypothesis is true* and a clear alternative *if the hypothesis is false*.
   - Uses conditional breakpoints or watchpoints when the failure occurs only on a specific iteration.
   - Leaves no ambiguity: after executing the plan, the hypothesis should be definitively true or false.

4. **Execute the plans in order of diagnostic power.** Prefer plans that can eliminate multiple hypotheses at once (e.g., inspecting a single data structure that both Hypothesis A and Hypothesis B depend upon). If the first plan confirms a hypothesis, skip the remaining plans for that failure and proceed to fix.

5. **After the fix is applied, re‑run the test.** If the test passes, the hypothesis is proven. If it fails again, return to step 1 and reformulate remaining hypotheses (if any) or generate new ones based on the new evidence.

**Example (abbreviated):**

```
Test failure: SAD8 AVX2 kernel returns incorrect sum for aligned inputs.

Hypothesis 1: The down‑counting loop terminates one iteration early.
  Plan: Set breakpoint at sad8_avx2_loop_exit. Check ecx (loop counter).
        If ecx != 0 → hypothesis confirmed (early exit).
        If ecx == 0 → hypothesis rejected (loop completed fully).

Hypothesis 2: The horizontal add reduction uses wrong permutation.
  Plan: Break after the first `vphaddd` instruction. Inspect YMM0 lanes.
        If lane values are not pairwise sums of input → confirmed.
        If lane values are correct → rejected.

Hypothesis 3: Input data is misaligned, causing `vmovdqa` to fault or load garbage.
  Plan: Break at entry. Examine RSI and alignment flags.
        If (RSI % 32) != 0 → confirmed; else rejected.
```

### Crash Analysis (SEGFAULT)

1. **Run under GDB**: `gdb --args <bin> <args>`
2. **Get the faulting instruction** from the backtrace: which instruction causes the SEGV?
3. **Identify the faulting operand**: e.g., `vpsubw (%rsi), %xmm0, %xmm0` → the crash reads from `[rsi]`. That register is your top priority.
4. **Trace the faulting register backward**: where was it set? What struct field or calculation produced that value?
5. **Check memory accessibility**: Use `x/8gx <address>` in GDB to confirm the address is mapped/unmapped.
6. **Ignore other anomalous register values** until the faulting operand is fully explained. Non-causal anomalies are distractions.

### Bit-Inexact or Wrong Results

1. **Isolate the bug**: Temporarily disable the ASM registration (`#if 0` the function pointer assignment). Verify the test passes without your change — this proves the test infrastructure is sound.
2. **Re-enable** and narrow down which input pattern fails.
3. **Compare register state**: Use GDB breakpoints at equivalent points in the ASM and C++ code paths. Compare key register values.
4. **Cross-validate struct offsets**: Compare your ASM's struct field accesses against a known-working ASM function that accesses the same fields (e.g., SAD functions for DistParam fields).

### Performance Regression (ASM slower than C++)

1. **Disassemble both** with `objdump -d` and compare instruction counts per functional block.
2. **Check for missing optimizations**:
   - Memory-folded operations (e.g., `vpaddd ymm0, ymm1, [mem]` instead of separate load+add)
   - Excess register spills (temporary stores to stack)
   - Loop structure differences (unrolling, alignment)
3. **Use `#if 0` to binary-search** which block causes the regression. Disable blocks one at a time until performance matches C++.
4. **Consider a fallback wrapper**: If ASM only accelerates a subset of calling contexts (e.g., specific block sizes), write a C++ wrapper that checks dimensions and delegates to C++ for unhandled cases:
   ```
   static FpDistFunc g_orig = nullptr;
   static Distortion wrapper(const DistParam& dp) {
     if (dp.org.width == 8 && dp.org.height == 8)
       return asm_function(&dp);
     return g_orig(dp);
   }
   ```
   Save the original function pointer before replacing it.

### Debug Signal Prioritization

When investigating any ASM issue, prioritize signals in this order:
1. **CRITICAL**: The faulting instruction's memory operand (crash) or the largest instruction count diff (performance)
2. **HIGH**: Control flow structure (loop counts, block boundaries)
3. **MEDIUM**: Register assignment and scheduling differences
4. **LOW**: Cosmetic instruction selection differences

Do NOT investigate all anomalies equally. Focus on the single most impactful signal until it is fully explained, then move to the next.

## Design Principles

- **Spec first, then implement** — Every optimization starts by generating a technical specification of the C++ compiler's output. The compiler is the reference, not our intuition. Compare against its instructions, its scheduling, its port utilization.

- **Measure against C++ baseline, not against previous ASM** — The C++ SIMD reference is the true baseline. If our ASM is slower than the compiler's output, we need to understand why. If it's equal or faster, we've succeeded. Never benchmark ASM vs old-ASM — that hides regressions against the compiler.

- **Every hypothesis is an experiment** — Before writing ASM, write a mini-spec for the hypothesis: what structural change is proposed, why it should be faster, which µarch bottleneck it addresses, and the expected instruction/cycle savings.

- **Benchmark with `-fno-inline` and volatile function pointers** — The C++ function is `static` in a header and will be inlined into the benchmark harness unless explicitly prevented. Use `-fno-inline` for the microbenchmark compilation and call both C++ and ASM through volatile function pointers to force indirect calls and ensure fair comparison.

- **Document negative results** — When a hypothesis fails to improve performance, save the experiment to `perf/experiments/`. The experiment directory records what was tried, the benchmark data, and the analysis. Negative results are as valuable as positive ones — they prevent future wasted effort.

- **Significance depends on environment** — On a workstation with turbo disabled: ~5% threshold. On a laptop with uncontrolled turbo/noise: ~15-20% threshold. Always state the threshold and the number of runs used.

- **Microbenchmarks isolate the function from the full encode pipeline** — The compiler's function pointer dispatch hides improvements smaller than ~5% of the function's time. Full encoder wall-clock comparisons are even noisier.

- **Validate bit-exactness** — ASM output must match C++ SIMD output exactly for all test patterns. Bit-exactness is non-negotiable.

- **x265 is reference, not template** — Adapt algorithms to VVC data structures rather than blindly copying x265 patterns.

- **Results persist in `.profiler/asm-optimizer/` and `perf/experiments/`**

- **NASM naming convention**: `vvenc_<operation>_<size>_<isa>.asm`

- **Registration** via `setupAssemblyPrimitives()` in `x86/asm-primitives.cpp`
