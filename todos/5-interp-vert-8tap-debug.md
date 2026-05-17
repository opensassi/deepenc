# interp-vert-8tap-debug

GitHub Issue: https://github.com/opensassi/deepenc/issues/5

## Previous Work

### What Succeeded

- `interp.filterHor` — horizontal 8-tap NASM AVX2, bit-exact, 0.934x perf, registered and building
- NASM build infrastructure in `CMakeLists.txt` (`.asm` files via custom commands, `-f elf64 -Ox`, LTO disabled)
- `asm-primitives.h`/`.cpp` registration pattern established
- Source file `asm-interp-vert-8tap.asm` compiles, links, runs without SIGILL
- `{vex3}` prefix mechanism identified and applied to all YMM instructions

### What Was Tried

- GAS vs NASM: GAS had VEX 2-byte/3-byte encoding issues; switched to NASM `.asm` with `{vex3}` prefix
- vpackssdw source order verified: `packs_epi32(a,b)` → per-lane `[a0-3, b0-3, a4-7, b4-7]`
- Column-group pointer reset: `r9` needed reset between groups
- Height reload: `r15d` needed reload from `[rbp+56]` between column groups (was decrementing to 0 permanently)

### What Remains

1. Row buffer init mismatch — C++ loads N-1 rows before loop + Nth inside; ASM loads all N before + N+1th inside (extra out-of-bounds load on last iteration)
2. Off-by-1 to off-by-8 rounding — ASM and C++ differ by 1-8 on random 10-bit data (suspected accumulator order or coefficient handling)
3. Validate stride multiples *3, *5, *6, *7 (NASM restricts indexed addressing to 1,2,4,8 — `lea` chaining needed)
4. Missing `.note.GNU-stack` section in `asm-interp-vert-8tap.asm`
5. Build vertical-specific microbenchmark harness following horizontal pattern

### Key Technical Details

- C++ reference: `InterpolationFilterX86.h:1288-1372` (`simdInterpolateVerM16_AVX2`)
- ASM target: `source/Lib/CommonLib/x86/asm-interp-vert-8tap.asm`
- Stack layout: `[rbp+48]=ret, [rbp+56]=height, [rbp+64]=coeffs`
- `{vex3}` rule: ALL YMM instructions using ymm0-ymm7 need `{vex3}` prefix, or NASM emits 128-bit VEX 2-byte encoding
- vpackssdw per-lane: `[a0-3, b0-3, a4-7, b4-7]`
- Horizontal ref: `asm-interp-horiz-8tap.asm` (bit-exact)
- x265 ref: `external/x265/source/common/ipfilter8.spec.md` (luma vertical)
