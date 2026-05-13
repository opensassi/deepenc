---
name: interp-vert-8tap-debug-5
description: Debug vertical 8-tap AVX2 interpolation filter bit-exactness (GitHub issue #5)
---

# Skill: interp-vert-8tap-debug-5

## Issue Reference

GitHub Issue: https://github.com/opensassi/deepenc/issues/5

## Dependencies

Requires: **asm-optimizer** — load this skill first via `skill asm-optimizer`.

Requires: **git** — load for commit workflow via `skill git`.

## Previous Work

### What Succeeded

- `interp.filterHor` — horizontal 8-tap NASM AVX2, bit-exact, 0.934x perf, registered and building
- NASM build infrastructure working in `CMakeLists.txt` (`.asm` files auto-compiled via custom commands with `-f elf64 -Ox`, LTO disabled via `vvenc_x86_simd` object library)
- `asm-primitives.h`/`.cpp` registration pattern established with `extern "C"` declarations and `reinterpret_cast` registration
- Source file `asm-interp-vert-8tap.asm` compiles, links, and runs without SIGILL
- `{vex3}` prefix mechanism identified and applied to all YMM instructions

### What Was Tried

- **Vertical filter architecture**: Load 8 rows (16 Pels each), process 4 coefficient pairs with `vpunpcklwd`/`vpunpckhwd` split, accumulate low/high accumulators, shift/pack/store, slide buffer
- **GAS vs NASM**: GAS (`asm-interp-horiz-8tap.cpp` with inline asm, then `.s` file) had VEX 2-byte/3-byte encoding issues. Switched to NASM `.asm` files which support `{vex3}` prefix
- **vpackssdw source order**: `_mm256_packs_epi32(suma, sumb)` = `{vex3} vpackssdw ymmDest, ymmSuma, ymmSumb` — per-lane output is `[suma0-3, sumb0-3, suma4-7, sumb4-7]`
- **Column-group pointer reset**: `r9` (next-row pointer) needed reset between column groups
- **Height reload**: `r15d` needed reload from `[rbp+56]` between column groups (was being decremented to 0 permanently)

### What Remains

1. Row buffer initialization — C++ loads N-1 rows before loop + Nth row inside loop; ASM loads all N before loop + (N+1)th inside loop. This causes an extra out-of-bounds load on the last iteration.
2. Off-by-1 to off-by-8 rounding — ASM and C++ values differ by 1-8 on random 10-bit data. Root cause unclear but suspected in accumulator order or coefficient handling.
3. Validate address calculation for stride multiples *3, *5, *6, *7 (NASM restricts indexed addressing to scale factors 1,2,4,8 — must use `lea` chaining)
4. Missing `.note.GNU-stack` section in `asm-interp-vert-8tap.asm`
5. Build vertical-specific microbenchmark harness following horizontal pattern

## Persona

Senior performance engineer with deep expertise in x86 assembly optimization, SIMD kernels, and video codec interpolation filters (DCT-IF). Familiar with VVenC's `InterpolationFilterX86.h` template hierarchy and NASM's VEX encoding behavior.

## On Activation

1. Load the `asm-optimizer` skill for the full toolkit
2. Read the C++ reference: `source/Lib/CommonLib/x86/InterpolationFilterX86.h:1288-1372` (simdInterpolateVerM16_AVX2)
3. Read the current implementation: `source/Lib/CommonLib/x86/asm-interp-vert-8tap.asm`
4. Read the bit-exact reference (horizontal): `source/Lib/CommonLib/x86/asm-interp-horiz-8tap.asm`
5. Build and test to reproduce the mismatch

## Commands

- `setup` — rebuild via `cmake --build build/relwithdebinfo-static -j$(nproc) --target vvenc`
- `test` — compile and run the vertical filter test:
  ```
  g++ -O0 -mavx2 source/Lib/CommonLib/x86/InterpolationFilterX86.h ...
  /tmp/test_vert_clean
  ```
- `gdb-trace <phase>` — debug specific phase (row-load, compute, pack, store)
- `fix <strategy>` — apply known fix strategy (buffer-init, accumulator-order, stride-compute)
- `bench` — create and run microbenchmark with `perf stat`
- `report-fix` — validate bit-exactness, update issue, close

## Debugging Context

### Known-Correct Intermediate Values (all-1 coeffs, all-100s src, width=16, height=64, stride=32)

After row loop entry (first iteration):
- Row 0: `[rsi]` = 100 x 16 Pels
- Row 1: `[rsi + rdx]` = 100 x 16 Pels
- etc. (all 100s)

ymm14 (even result after vpsrad 6) `v8_int32`:
- `{-403, 53, 108, 112, 116, 120, 124, 128}` (with original coeffs, ramp input)

ymm15 (odd result after vpsrad 6):
- `{290, 106, 110, 114, 118, 122, 126, 130}`

### vpackssdw Behavior (from test_pack.cpp)
```
_mm256_packs_epi32(a={0..7}, b={8..15}) → {0,1,2,3, 8,9,10,11, 4,5,6,7, 12,13,14,15}
```
Per 128-bit lane: `[a0-3, b0-3, a4-7, b4-7]`

### {vex3} Prefix Rule
ALL YMM instructions using registers ymm0-ymm7 need `{vex3}` prefix in NASM, otherwise NASM silently emits VEX 2-byte (128-bit) encoding, zeroing the upper 128 bits of the destination register. Registers ymm8-ymm15 always get VEX 3-byte automatically.

### Stack Layout (after 6 pushes + `mov rbp, rsp`)
```
[rbp+48] = return address
[rbp+56] = height (arg7)
[rbp+64] = coeff ptr (arg8)
```

## Files Reference

| File | Role |
|------|------|
| `source/Lib/CommonLib/x86/asm-interp-vert-8tap.asm` | Target ASM implementation (to be fixed) |
| `source/Lib/CommonLib/x86/asm-interp-horiz-8tap.asm` | Bit-exact reference NASM implementation |
| `source/Lib/CommonLib/x86/InterpolationFilterX86.h:1288` | C++ reference: `simdInterpolateVerM16_AVX2` |
| `source/Lib/CommonLib/x86/asm-primitives.h` | `extern "C"` declarations |
| `source/Lib/CommonLib/x86/asm-primitives.cpp` | Registration in `setupAssemblyPrimitives()` |
| `source/Lib/vvenc/CMakeLists.txt` | NASM build integration |
| `.profiler/asm-optimizer/microbenchmarks/interp-filterHor/` | Horizontal filter benchmark (reference for creating vertical bench) |

## Design Principles

- **NASM, not GAS**: Always use `.asm` files with `default rel`, never GAS inline asm.
- **`{vex3}` on all low-reg YMM**: Any instruction with ymm0-ymm7 MUST have `{vex3}` prefix.
- **Verify with `objdump -d`**: Check that YMM instructions show `c4` VEX 3-byte prefix (not `c5`).
- **Bit-exactness is non-negotiable**: ASM output must match C++ SIMD output exactly.
- **Benchmark against C++ baseline**: Not against previous ASM. Use `-fno-inline` and volatile function pointers.
- **x265 reference**: `external/x265/source/common/v4-ipfilter8.spec.md` for vertical chroma; `external/x265/source/common/ipfilter8.spec.md` for luma vertical.
