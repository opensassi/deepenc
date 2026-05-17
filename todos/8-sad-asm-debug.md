# sad-asm-debug

GitHub Issue: https://github.com/opensassi/deepenc/issues/8

## Previous Work

### What Succeeded

- QuantRDOQ/DepQuant sub-array cache optimization (bit-exact, ~3.1% faster at medium preset)

### What Was Tried

- Running vvencapp with `--preset fast` in debug → consistent SEGV in `vvenc_sad32_avx2`
- Same command in release → identical bitstream, no crash
- Adding `--threads 1`, disabling ASM flags → crash persisted

### What Remains

- Compare `sizeof(DistParam)` between debug and release builds to find offset mismatch
- Fix inline assembly to be layout-agnostic (`offsetof` constants or C++ wrapper)
- Verify all SAD sizes (8/16/32/64) pass bit-exact in debug
- Confirm no release regression

### Key Technical Details

- Crash: `vpsubw ymm2, ymm1, [rax]` — rax points to text section, not pixel data
- Hardcoded offsets in asm (asm-sad_avx2.cpp:103-108):
  - `[rdi + 0x44]` → ecx (shift amount) — likely correct
  - `[rdi + 0x10]` → r9d (src stride) — likely correct
  - `[rdi + 0x28]` → r8d (dst stride) — likely correct
  - `[rdi + 0x04]` → r11d (height) — **garbage** (struct layout mismatch)
  - `[rdi + 0x08]` → rdx (src buf) — valid
  - `[rdi + 0x20]` → rax (dst buf) — **text section** (struct layout mismatch)
- Suspect: `DistParam` struct layout differs between debug/release (padding or CPelBuf member order)
- Files: `source/Lib/CommonLib/x86/avx2/asm-sad_avx2.cpp`, `RdCost.h:82`, `Buffer.h`
