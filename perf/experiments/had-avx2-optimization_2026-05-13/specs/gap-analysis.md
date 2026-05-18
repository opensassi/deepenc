## Gap Analysis: ASM vs C++ SIMD for HAD 8x8

### C++ Reference: `xCalcHAD8x8_SSE` (RdCostX86.h:655)

Computes 8×8 Hadamard using SSE intrinsics (`__m128i`):
1. Load 8 rows of 8 Pels from org and ref → 16-bit differences
2. Stage 1 butterfly (vertical): add/sub pairs across row distance N/2
3. Stage 2 butterfly: add/sub across row distance N/4
4. Stage 3 butterfly: add/sub across row distance N/8 (= final vertical transform)
5. Unpack to interleave transform-domain coefficients (vpunpcklwd/hwd/ldq/hdq)
6. Sign-extend to 32-bit (vpmovsxwd)
7. Repeat butterfly + accumulation for rows now in columns (horizontal transform)
8. vpabsd → accumulate → round with `>> DISTORTION_PRECISION_ADJUSTMENT`

### ASM: `vvenc_had8x8_kernel`

Same algorithm, hand-scheduled for register pressure and port utilization.

### Comparison by Functional Block

| Block | C++ (intrinsics) | ASM (instructions) | Diff | Impact |
|-------|------------------|--------------------|------|--------|
| Load diff | 8 loads × 2 (org+ref) + 8 vpsubw | 8 vmovdqu + 8 vpsubw | Same | None |
| Vertical butterfly ×3 | 24 vpaddw/vpsubw | 36 vpaddw/vpsubw | +12 | High — C++ uses interleaved add/sub in fewer regs |
| Transpose | 24 punpck | 22 vpunpck | -2 | Low |
| Sign-extend | 16 pmovsxwd (implicit via compiler) | 16 vpmovsxwd + 8 vpsrldq | +8 | Medium — C++ keeps upper/lower halves implicitly |
| Horizontal butterfly ×3 | 24 vpaddd/vpsubd | 36 vpaddd/vpsubd | +12 | High — same issue as vertical |
| vpabsd | 8 | 16 | +8 | Medium — ASM splits into two passes |
| Accumulate | scalar accumulation + shift | vphaddd + scalar rounding | Same | None |

### Key Structural Differences

| # | Difference | C++ Approach | ASM Approach | Estimated µops | Priority |
|---|------------|-------------|--------------|----------------|----------|
| 1 | **Two-pass abs/add** | vpabsd, then vpaddd in same register | Saves to stack, reloads, adds from stack | +16 µops (8 stores + 8 loads) | High |
| 2 | **Extra register copies** | Compiler uses registers directly | Explicit `vmovdqu` copies (`xmm8` ← `xmm0` at line 82-89) | +8 µops | Medium |
| 3 | **Stack frame alloc** | Compiler manages with push/push | `sub rsp, 256` + 256 bytes of stack spills | +64 µops (allocation + 32 vmovdqu stores/loads) | Critical |
| 4 | **Manual transposition** | Compiler schedules unpack ops | Same unpack structure, but with explicit register moves | Negligible | Low |

### Bottleneck Assessment

The 256-byte stack frame is the dominant cost. The C++ compiler can keep all temporary values in registers (16 XMM registers available for 8×8 processing). The ASM kernel explicitly spills to stack because:
- The 8×8 transpose requires all 16 XMM registers simultaneously (8 for coefficients + 8 for intermediate results)
- After swap-out spill, the vertical/horizontal passes also need registers, forcing additional spills

### µarch Analysis (Sunny Cove / Skylake)

| Resource | Availability | ASM Usage | Pressure |
|----------|--------------|-----------|----------|
| P0 (alu/vec) | 1/cycle | vpaddw/subd/absd | Moderate |
| P1 (alu/vec) | 1/cycle | vpunpck, vpmovsxwd | Moderate |
| P5 (vec/shuffle) | 1/cycle | vpsrldq, vmoxdqu stores | High (stores + shuffles compete) |
| P2/P3 (load) | 2/cycle | vmovdqu loads | Low (only at top of kernel) |
| P4 (store) | 1/cycle | vmovdqu stores | High (32 stores in middle section) |
| L1D bandwidth | 64B/cycle | Peak ~128B stored then reloaded | Contended during spill/reload section |

### Improvement Opportunities

1. **Replace stack spills with XMM register renaming**: The `vmovdqu xmm8, xmm0` at line 82-89 are unnecessary — downstream operations can directly use `xmm8`-`xmm15` without copying from `xmm0`-`xmm7`. The compiler avoids this entirely.

2. **Single-pass abs+add**: The two-pass abs-then-add (lines 146-161 then 162-209) can be fused: compute `abs(x)` and add to accumulator in one pass, eliminating 16 stack stores and 16 reloads.

3. **Eliminate `sub rsp, 256`**: The 256-byte frame is only used for the two-pass spill. If fused into single-pass, the stack allocation can be removed entirely (or reduced to ~64 bytes for rounding temporaries).

### Estimated Maximum Improvement

With optimizations 1-3, the ASM kernel would save ~80-100 µops (from ~140 to ~50-60), potentially reaching ~1.2-1.3x vs C++. However, 7% on a microbenchmark translates to sub-1% at the encoder level (HAD is ~3-5% of encode time on medium preset).
