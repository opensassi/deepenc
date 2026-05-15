---
name: sad-asm-debug-8
description: Debug SAD32 AVX2 inline assembly struct-offset mismatch that crashes debug builds (GitHub issue #8)
---

# Skill: sad-asm-debug-8

## Issue Reference

GitHub Issue: https://github.com/opensassi/deepenc/issues/8

## Dependencies

Requires: **asm-optimizer** — load this skill first via `skill asm-optimizer`.

## Previous Work

### What Succeeded
- QuantRDOQ/DepQuant sub-array cache optimization (bit-exact, ~3.1% faster at medium preset)

### What Was Tried
- Running vvencapp with --preset fast in debug → consistent SEGV in vvenc_sad32_avx2
- Same command in release → identical bitstream, no crash
- Adding --threads 1, disabling ASM flags → crash persisted

### What Remains
- Compare sizeof(DistParam) between debug and release builds to find offset mismatch
- Fix inline assembly to be layout-agnostic (offsetof constants or C++ wrapper)
- Verify all SAD sizes (8/16/32/64) pass bit-exact in debug
- Confirm no release regression

## Persona

Senior x86 assembly engineer with deep expertise in GCC inline assembly, struct layout, and ABI compatibility between debug and release modes.

## On Activation

1. Compile a small test program that prints sizeof(DistParam), sizeof(CPelBuf), and field offsets in both debug and release configurations.
2. Identify which hardcoded offset in the SAD32 assembly mismatches.
3. Apply the fix.

## Commands

- `setup` — rebuild debug + release, warmup encode
- `gdb-trace` — run SAD32 under GDB and print DistParam fields at crash point
- `fix-offsets` — replace hard offsets with offsetof-based constants or C++ wrapper
- `test` — run bit-exact comparison debug vs release
- `report-fix` — validate, wire, close issue

## Debugging Context

Crash point: vvenc_sad32_avx2
Register at crash (GDB):
  rdi = 0x7ffff6dfa8c0  (DistParam*, valid heap)
  rax = 0x555555bcf311   (text section — should be pixel data!)
  rdx = valid src buffer
  r11d = remaining rows
Faulting insn: vpsubw ymm2, ymm1, [rax]  (line 120 of asm-sad_avx2.cpp)

Current hardcoded offsets in the asm (asm-sad_avx2.cpp:103-108):
  [rdi + 0x44] → ecx (shift amount)
  [rdi + 0x10] → r9d (src stride?)
  [rdi + 0x28] → r8d (dst stride?)
  [rdi + 0x04] → r11d (height) — loaded GARBAGE
  [rdi + 0x08] → rdx (src buf) — valid
  [rdi + 0x20] → rax (dst buf) — points to text section!

The [rdi + 0x04] and [rdi + 0x20] offsets suggest the struct layout differs
between debug and release (likely extra padding or different CPelBuf member order).

## Files Reference

| File | Role |
|------|------|
| source/Lib/CommonLib/x86/avx2/asm-sad_avx2.cpp | Inline assembly with hardcoded offsets (lines 103-108) |
| source/Lib/CommonLib/RdCost.h | DistParam class definition (line 82) |
| source/Lib/CommonLib/Buffer.h | CPelBuf struct definition |
| source/Lib/CommonLib/x86/asm-primitives.cpp | Assembly registration (line 221) |

## Design Principles

- Start by compiling an offset-checking C++ program in debug and release to find the exact mismatch.
- Prefer offsetof() constants over C++ wrapper if the struct is stable across configurations.
- Bit-exactness is non-negotiable — both debug and release must produce identical SAD results.
