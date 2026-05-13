---
name: dq-asm-minselect-debug-3
description: Debugging extension for asm-optimizer — fixes the dq.checkAllRdCosts ASM min-select blend mask bug (GitHub issue #3)
---

# Skill: dq-asm-minselect-debug-3

## Issue Reference

GitHub Issue: https://github.com/opensassi/deepenc/issues/3

## Dependencies

Requires: **asm-optimizer** — load this skill first via `skill asm-optimizer`.
This skill extends asm-optimizer with debugging commands for the specific
`dq.checkAllRdCosts` min-select blend mask bug.

## Persona

Senior performance engineer with deep expertise in x86 SIMD assembly debugging,
specialized in VVenC's DepQuant (dependent quantization) TCQ state machine
and its AVX2 dispatch table implementation.

## On Activation

1. Load the asm-optimizer skill (warn if not already loaded)
2. Fetch the issue body via `gh issue view 3 --repo opensassi/deepenc` and display current status
3. Read the current asm implementation at `source/Lib/CommonLib/x86/asm-dq-checkAllRdCosts.cpp`
4. Check the microbenchmark at `.profiler/asm-optimizer/microbenchmarks/dq-checkAllRdCosts/`
5. Run setup commands to rebuild and test
6. Show the available debugging commands

## Commands

### `setup`

Rebuild the full project and microbenchmark:

```
cmake --build build/release-static -j$(nproc) --target vvencapp
.profiler/asm-optimizer/microbenchmarks/dq-checkAllRdCosts/build.sh
```

### `test`

Run the bit-exact comparison against the C++ SIMD reference:

```
taskset -c 0 .profiler/asm-optimizer/microbenchmarks/dq-checkAllRdCosts/dq_microbench
```

### `gdb-trace <phase>`

Launch GDB with pre-configured breakpoints for a specific phase of the
3-way min-select.

| Phase | Breakpoint Offset | What to Inspect |
|-------|-------------------|-----------------|
| `valcand2` | +0x42c | Loads: r14d/r15d/r8d/r9d from pqData; valCand2 in xmm13 |
| `blend1` | +0x407 | chng mask (xmm0), valCand1 (xmm13), valBest (xmm11) before round 1 blend |
| `blend2` | +0x468 | chng2 mask (xmm0), valCand2 (xmm13), valBest (xmm11) before round 2 blend |
| `pack` | +0x46e | xmm11 (valBest i32) before vpackssdw, then after as i16 |

Usage:
```
gdb -batch -ex "break *vvenc_dq_checkAllRdCosts_avx2+0x42c" \
  -ex "run" \
  -ex "printf \"valCand2 xmm13: \"" \
  -ex "print/x \$xmm13.v4_int32" \
  .profiler/asm-optimizer/microbenchmarks/dq-checkAllRdCosts/dq_microbench 2>/dev/null
```

### `fix <strategy>`

Apply a known-fix strategy to the blend mask logic in `.L_select_asm`:

- `fix --blend-order` — Swap operand order in `vpblendvb` to verify blend direction
- `fix --shuffle-mask` — Try different `vpshufd` immediate values
- `fix --use-pblendd` — Replace `vpblendw`+`vpshufd` with a single `vpblendd` (32-bit granular blend, avoids byte-level mask confusion)
- `fix --scalar-fallback` — Replace the asm min-select with a C++ helper function (keeps rdCost computation in asm, only selection in C++)

### `bench`

Run microbenchmark under perf stat and compare against stored baseline:

```
taskset -c 0 perf stat -d -d -d \
  .profiler/asm-optimizer/microbenchmarks/dq-checkAllRdCosts/dq_microbench 2>&1
```

### `report-fix`

After the fix is confirmed passing `test`:

1. Run `bench` to measure IPC improvement vs baseline
2. `gh issue close 3 --repo opensassi/deepenc --comment "Fix: <summary of what was changed>"`

## Debugging Context (from original implementation session)

### Known-Correct Values (test[0], spt=ISCSBB, first invocation)

| Register/Field | Value | Source |
|----------------|-------|--------|
| pq[0].absLevel | 6 | `*(short*)$rsi` |
| pq[1].absLevel | 5 | `*(short*)(rsi+16)` |
| pq[2].absLevel | 6 | `*(short*)(rsi+32)` |
| pq[3].absLevel | 4 | `*(short*)(rsi+48)` |
| rdCost ref[0] | 440399444735327418 | matches ASM |
| rdCost ref[2] | 440399444735388614 | matches ASM |
| rdCost ref[1] | 286936285662404593 | matches ASM |
| rdCost ref[3] | 286936285662443023 | matches ASM |
| valCand2 (xmm13) | {0x6, 0x5, 0x6, 0x4} | verified at breakpoint +0x42c |
| idxCand2 (xmm4) | {0x1, 0x3, 0x1, 0x3} | from .L_idxCand2_asm |

### Buggy Values at Pack (+0x46e)

| Register (BEFORE pack) | Value | Expected |
|------------------------|-------|----------|
| valBest (xmm11) i32 | {0x8000, 0x8000, 0x0, 0x8005} | {0, 0, 6, 5} |
| idxBest (xmm12) i32 | {0x0, 0x2, 0x1, 0x2} | {0, 2, 0, 2} |
| chng2 (xmm0) | garbage (not all-0s/all-1s) | {0/FF, 0/FF, 0/FF, 0/FF} |
| valCand2 (xmm13) | {0x8000, 0x8000, 0x8000, 0x8000} | {6, 5, 6, 4} |

**Key observation:** valCand2 is correct at +0x42c but shows all-0x8000 at +0x46e.
This suggests xmm13 gets clobbered between construction and blend. The only
instructions between valCand2 construction and the round 2 blend are:
`vmovdqu xmm4, [rip + .L_idxCand2_asm]` (loads idxCand2 into xmm4, not xmm13).
Check if the `vpcmpgtq` for round 2 or the `vpblendw`/`vpshufd` for the chng2 mask
accidentally write to xmm13. If not, the issue may be a register renaming or
pipeline hazard.

### Bugs Already Fixed (do not regress)

| # | Bug | Fix |
|---|-----|-----|
| 1 | deltaDist PQData offset 24→40 | pq[2].deltaDist is at rsi+40, not +24 |
| 2 | deltaDist PQData offset 12→24 | pq[1].deltaDist is at rsi+24 |
| 3 | deltaDist PQData offset 40→56 | pq[3].deltaDist is at rsi+56 |
| 4 | rbx/r12-r15 not saved/restored | push/pop in prologue/epilogue |
| 5 | Missing `add rsp,64` before pop rbp | Stack corruption on return |
| 6 | `mov rbx,[rsp+N]` reading 8 bytes for 32-bit values | Changed to `mov ebx,[rsp+N]` |
| 7 | `vpmovzxwd` (16→32) for sigBits | Changed to `vpmovzxdq` (32→64) |
| 8 | `vpmovzxwd` for sbbBits | Changed to `vpmovsxdq` (signed 32→64) |
| 9 | Round 2 min-select missing | Added valCand2+compare+blend |
| 10 | Redundant `vmovdqu xmm4` for idxCand overwritten by idxCand2 | Removed dead load |

### Function Offset Map

Offsets relative to `vvenc_dq_checkAllRdCosts_avx2` from `objdump -d`:

| Offset | Instruction | Purpose |
|--------|-------------|---------|
| 0x000 | push r12 | Prologue start |
| 0x0d9 | movswl (%rsi),%r14d | cffBits A pq[0] |
| 0x18f | movswl (%rsi),%r14d | valCand1 pq[0] |
| 0x3d4 | movswl 0x20(%rsi),%r14d | valBest init pq[2] |
| 0x3e3 | vpinsrd $0x2,%r14d,%xmm11,%xmm11 | valBest[2] = pq[2] |
| 0x3e9 | vpinsrd $0x3,%r15d,%xmm11,%xmm11 | valBest[3] = pq[1] |
| 0x407 | vpblendvb | Round 1 valBest blend |
| 0x42c | movswl 0x20(%rsi),%r14d | valCand2 pq[2] |
| 0x431 | movswl 0x10(%rsi),%r15d | valCand2 pq[1] |
| 0x436 | movswl (%rsi),%r8d | valCand2 pq[0] |
| 0x43a | movswl 0x30(%rsi),%r9d | valCand2 pq[3] |
| 0x46e | vpackssdw | First pack instruction |
| 0x49e | ret | Function end |

## Design Principles

- Every fix must be validated by `test` (bit-exact comparison against C++ SIMD reference)
- rdCost must NOT regress when absLevel/prevId are fixed
- Use GDB breakpoints at known objective offsets (from objdump), not guessed addresses
- Prefer minimal surgical changes to the asm string
- If the asm fix proves intractable, fall back to a C++ helper for the min-select only
- Document every attempt whether successful or not
- The fix should be benchmarked with `bench` before closing
