---
name: had-avx2-optimization-4
description: Debugging extension for asm-optimizer — completes the HAD (SATD) 8x8 and 16x16 AVX2 kernels with explicit register allocation (GitHub issue #4)
---

# Skill: had-avx2-optimization-4

## Issue Reference

GitHub Issue: https://github.com/opensassi/deepenc/issues/4

## Dependencies

Requires: **asm-optimizer** — load this skill first via `skill asm-optimizer`.
This skill extends asm-optimizer with debugging and implementation commands for
the `xCalcHAD8x8_SSE` and `xCalcHAD16x16_AVX2` kernel optimization.

## Previous Work

### What Succeeded
SAD ASM for all 4 block sizes (8/16/32/64) was completed, accepted, and wired into
the encoder dispatch via `applyRdCostAsmOverrides()` in `asm-sad_avx2.cpp`. The
per-instance `m_afpDistortFunc` override pattern was proven and can be reused for HAD.

### What Was Tried (HAD)
A partial `vvenc_had8x8_sse2` ASM implementation was written as GAS inline assembly
in `perf/experiments/sad-avx2-port_2026-05-12/had-todo/had_microbench.cpp`.
The algorithm up through transpose and sign-extension was verified correct.
The horizontal butterfly stage-3 produces incorrect results due to a data dependency
bug: stage-3 reads stage-2 values that have already been modified by stage-3 itself.

### What Remains
1. Fix the stage-3 butterfly: all 8 results must be computed from **original** stage-2
   values before any are stored (use temporary variables for final pair results)
2. Complete the lo/hi reorganization (g0=[lo0-3,hi0-3], g1=[lo4-7,hi4-7]) before butterfly
3. Write `vvenc_had16x16_avx2` (YMM variant, 2 passes of 8 rows each)
4. Benchmark and wire into encoder dispatch

## Persona

Senior performance engineer with deep expertise in x86 SIMD assembly, 2D Hadamard
transform optimization, and video codec SATD kernel design.

## On Activation

1. Load the asm-optimizer skill (warn if not already loaded)
2. Fetch the issue body via `gh issue view 4 --repo opensassi/deepenc` and display current status
3. Read the partial ASM at `perf/experiments/sad-avx2-port_2026-05-12/had-todo/had_microbench.cpp`
4. Read the C++ reference at `source/Lib/CommonLib/x86/RdCostX86.h:655-796` (xCalcHAD8x8_SSE)
5. Read the SAD ASM at `source/Lib/CommonLib/x86/avx2/asm-sad_avx2.cpp` for the wiring pattern
6. Run setup commands to rebuild and test
7. Show the available commands

## Commands

### `setup`

Rebuild the full project and microbenchmark:

```
cmake --build build/release-static -j$(nproc) --target vvencapp
bash perf/experiments/sad-avx2-port_2026-05-12/had-todo/build-had.sh
```

### `test`

Run the bit-exact comparison against the C++ SIMD reference:

```
taskset -c 0 perf/experiments/sad-avx2-port_2026-05-12/had-todo/had_microbench
```

### `gdb-trace <phase>`

Launch GDB with pre-configured breakpoints for a specific phase of the
8x8 Hadamard transform.

| Phase | Description | What to Inspect |
|-------|-------------|-----------------|
| `load` | After 8-row diff load | xmm0-xmm7 = diff per row |
| `vbutterfly` | After vertical 3-stage butterfly | xmm0-xmm7 = transformed rows |
| `transpose` | After 8x8 transpose | xmm0-xmm7 = column-major matrix |
| `signext` | After sign-extend 16->32 | lo[0..7] (low), hi[0..7] (hi) |
| `butterfly` | After horizontal 32-bit butterfly | Abs values in registers |
| `reduce` | After tree reduction + DC adjust | Final scalar result |

Usage:
```
gdb -batch -ex "break vvenc_had8x8_sse2" \
  -ex "run" \
  -ex "print/x \$xmm0.v8_int16" \
  perf/experiments/sad-avx2-port_2026-05-12/had-todo/had_microbench 2>/dev/null
```

### `fix <strategy>`

Apply a known-fix strategy to the butterfly scheduling bug:

- `fix --stage3-temps` — Use 8 temporary variables to hold stage-3 results, write back after all computed
- `fix --nasm-macros` — Rewrite as NASM `.asm` file using x265's HADAMARD macro from x86util.asm
- `fix --split-lo-hi` — Process lo and hi halves separately with register blocking

### `bench`

Run microbenchmark under perf stat:

```
taskset -c 0 perf stat -d -d -d \
  perf/experiments/sad-avx2-port_2026-05-12/had-todo/had_microbench 2>&1
```

### `report-fix`

After the fix is confirmed passing `test`:

1. Run `bench` to measure IPC improvement vs baseline
2. Move validated ASM from experiment dir to `source/Lib/CommonLib/x86/avx2/asm-sad_avx2.cpp`
3. Extend `applyRdCostAsmOverrides()` to register HAD ASM
4. `gh issue close 4 --repo opensassi/deepenc --comment "Fix: <summary of what was changed>"`

## Algorithm Reference (Verified Against C++ Reference)

### 8x8 Hadamard Data Flow

```
Load diff:       d[0..7] = loadu(piOrg + k*stride) - loadu(piCur + k*stride)
                 Each register = 8 int16 Pels per row

Vertical Stage 1:  s[0..3] = d[0..3] + d[4..7]     (sum)
                   s[4..7] = d[0..3] - d[4..7]     (diff)
Vertical Stage 2:  d[0..3] = s[0..3] +/- s[2,3]    (within groups)
                   d[4..7] = s[4..7] +/- s[6,7]
Vertical Stage 3:  s[0..7] = final pair butterfly   (d[0] +/- d[1], etc.)

Transpose 8x8:     unpack epi16 → epi32 → epi64     (3-step 8x8 transpose)

Sign-extend:       lo[i] = cvtepi16_epi32(tr[i])    (lower 4 words)
                   hi[i] = cvtepi16_epi32(shift tr[i] by 8)  (upper 4 words)

Reorganize:        g0 = [lo[0], lo[1], lo[2], lo[3], hi[0], hi[1], hi[2], hi[3]]
                   g1 = [lo[4], lo[5], lo[6], lo[7], hi[4], hi[5], hi[6], hi[7]]

Horizontal Stage 1: g[0..3] + g[4..7] → sums; g[0..3] - g[4..7] → diffs
Horizontal Stage 2: within pairs (8 regs → 8 new regs)
Horizontal Stage 3: final pair butterfly + abs → 8 abs values

                  * CRITICAL: Stage 3 must read original stage-2 values *

Combine:           abs_g0[i] + abs_g1[i] for i=0..7

Save DC:           abs_g0[0][0] (first dword of first column before combine)

Tree reduce:       pairwise sum → hadd to scalar → (sad - absDc + (absDc>>2) + 2) >> 2
```

### Register Allocation (XMM, 16 available)

| Phase | Registers Used | Purpose |
|-------|---------------|---------|
| Load+Diff | xmm0-xmm7 | 8 rows of diffs |
| V.Butterfly | xmm0-xmm7 + xmm8-xmm15 | 16 temps for 3-stage butterfly |
| Transpose | xmm0-xmm15 | 16 temps for 3-step transpose → output in xmm0-xmm7 |
| Sign-extend | xmm8-xmm15 (lo), stack+shift (hi) | 8 lo values → butterfly → abs → save to stack |
| H.Butterfly | xmm0-xmm7 (g0 or g1 regen'd) | 8 values for 3-stage butterfly |
| Combine | xmm0-xmm7 (abs) + stack restore | Combine g0_abs (stack) + g1_abs (regs) |

### Baseline Performance Targets

| Metric | C++ SIMD | ASM Target |
|--------|----------|------------|
| HAD8x8 ns/call | 21.85 | < 15 ns (>30% faster) |
| IPC (mixed) | 2.70 | > 3.2 |
| Backend bound | 50.0% | < 38% |
| L1 miss rate | 1.16% | < 0.05% |

## Debugging Context

### Known-Correct Intermediate Values (from C++ reference trace)

Input data (rng=42, first 8x8 block):
```
  diff[0]: 119 631 828 207 65417 65012 49 65414
  diff[1]: 65481 64632 335 285 246 49 65131 111
  diff[2]: 138 65305 65127 65146 80 29 65391 64989
  diff[3]: 523 415 566 64719 368 579 118 65113
  diff[4]: 400 64721 65422 848 691 65102 113 65180
  diff[5]: 484 65332 262 65134 535 196 146 65370
  diff[6]: 711 64965 405 453 65475 466 409 107
  diff[7]: 207 65358 199 64921 65330 88 65226 65284
```

After vertical butterfly (m1[0][k]):
```
  m1[0][0]: 2527 63679 2072 65105 1534 449 65511 63888
  m1[0][1]: 209 65421 64884 2667 65184 64161 877 65348
  m1[0][2]: 64905 64809 550 2307 1172 63661 65367 582
  m1[0][3]: 65507 1963 886 65213 65470 64505 65501 64878
  m1[0][4]: 64459 1679 568 64537 65152 65353 64795 65222
  m1[0][5]: 64905 1893 65224 63567 64582 64665 65041 65010
  m1[0][6]: 64973 65349 1462 1091 63722 65245 65047 1336
  m1[0][7]: 1147 2399 2050 64849 65448 985 1469 440
```

Reference result: `43376`

### Known Bug

The partial ASM at `had_microbench.cpp` produces result `8829` instead of `43376`.
This is ~20% of the correct value, suggesting only the lo-half butterfly is contributing
and the hi-half is lost or the reorganization is wrong. The primary fix is ensuring
stage-3's `vpaddd`/`vpsubd` on lines ~220-235 read from the original stage-2 register
values, not from registers that were already modified by an earlier stage-3 operation.

## Design Principles

- Every fix must be validated by `test` (bit-exact comparison against C++ SIMD reference)
- Use GDB breakpoints to verify register values at each pipeline stage against the
  known-correct values in the Debugging Context section
- Prefer NASM with x265 macros over GAS inline for complex butterfly code
- If pure-ASM proves intractable, fall back to a hybrid: C++ for the load+vertical+
  transpose phases (which the compiler handles reasonably), ASM only for the
  horizontal butterfly+abs+reduce (where the spills are worst)
- Document every attempt whether successful or not
- The fix should be benchmarked with `bench` before closing

## Files Reference

| File | Role |
|------|------|
| `source/Lib/CommonLib/x86/RdCostX86.h:655-796` | C++ reference: `xCalcHAD8x8_SSE` |
| `source/Lib/CommonLib/x86/RdCostX86.h:2002-2169` | C++ reference: `xCalcHAD16x16_AVX2` |
| `source/Lib/CommonLib/x86/avx2/asm-sad_avx2.cpp` | Existing ASM + override registration (add HAD here) |
| `source/Lib/CommonLib/RdCost.cpp` | Call site for `applyRdCostAsmOverrides()` |
| `perf/experiments/sad-avx2-port_2026-05-12/had-todo/` | Partial ASM + microbench |
| `perf/experiments/sad-avx2-port_2026-05-12/had-todo/had_microbench.cpp` | WIP ASM implementation |
| `external/x265/source/common/pixel-a.asm` | x265 SATD reference (16,583 lines) |
| `external/x265/source/common/x86/x86util.asm` | HADAMARD macro definitions |
