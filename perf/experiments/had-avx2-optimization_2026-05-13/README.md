## Experiment: HAD (SATD) AVX2 — 8x8 and 16x16

### Hypothesis

Hand-written GAS inline assembly can beat the C++ SIMD (`xGetHADs_SIMD` with `xCalcHAD8x8_SSE` / `xCalcHAD16x16_AVX2`) by eliminating register spills and using an explicit transposition schedule tuned for the exact block size.

### Kernels Attempted

| Kernel | Approach | Outcome | Speedup |
|--------|----------|---------|---------|
| 8x8 (XMM) | Single-pass 8×8 1D Hadamard on rows, transpose, 1D Hadamard on columns, DC-rounding adjustment | **Accepted** | 1.07x (+7%) |
| 16x16 (4×8x8 calls) | Decompose 16×16 into four 8×8 sub-blocks via wrapper, call the same 8x8 kernel | **Archived** | 0.69x (-31%) |

### 8x8 ASM Implementation

The `vvenc_had8x8_kernel` function:
1. Loads 8 rows × 8 Pels from source (`[rdi+rdx*row]`) and reference (`[rsi+rcx*row]`)
2. Computes 8-element differences (vpsubw)
3. Performs 1D Hadamard transform (vertical): 3-stage butterfly on columns
4. Transposes 8×8 matrix: 3-stage unpack (punpcklwd → punpckldq → punpcklqdq)
5. Sign-extends from 16-bit to 32-bit (pmovsxwd)
6. Performs 1D Hadamard transform (horizontal): 3-stage butterfly on rows
7. Takes absolute values (vpabsd)
8. Cross-adds both transform stages and rounds via `(sum - DC + DC/4 + 2) >> 2`

The `vvenc_had8_avx2` wrapper:
1. Reads DistParam fields (org.buf, cur.buf, org.stride, cur.stride, org.height)
2. Loops over 8-row batches, advancing pointers by `stride×8` bytes per iteration
3. Accumulates the sum in a stack slot `[rbp-8]`

### 16x16 ASM Implementation

The `vvenc_had16_avx2` wrapper decomposes 16×16 into 4 calls to the 8x8 kernel:
1. Call kernel on top-left 8×8 at `(r14, r15)`
2. Call kernel on top-right 8×8 at `(r14+16, r15+16)`
3. Call kernel on bottom-left 8×8 at `(r14+r12×16, r15+r13×16)`
4. Call kernel on bottom-right 8×8 at `(r14+r12×16+16, r15+r13×16+16)`

### Benchmark Results

Laptop, `taskset -c 0`, 200000 iterations, significance threshold ~15%:

| Case | C++ (ns) | ASM (ns) | Speedup | Verdict |
|------|----------|----------|---------|---------|
| 8×8 | 22.63 | 21.20 | **1.07x** | Below threshold, but merged as warm-target |
| 8×16 | 41.71 | 40.54 | 1.03x | Below threshold |
| 8×32 | 81.59 | 79.41 | 1.03x | Below threshold |
| 16×16 | 54.79 | 79.09 | **0.69x** | Archived — regression |

### 16×16 Root Cause

The 0.69x regression is caused by:
1. **4 function calls** via `call` instruction (each ~10-20 cycles for call/ret plus stack frame overhead)
2. **Per-call stack alloc**: each kernel call does `sub rsp, 256` (256-byte stack frame)
3. **No register reuse across sub-blocks**: registers are spilled/reloaded per 8×8 call
4. **C++ SIMD advantage**: `xCalcHAD16x16_AVX2` uses YMM registers in a single pass, processing 8 Pels per operation with no function call overhead

### Bugs Fixed During Development

1. **Transpose T.S3 column 5/6 swapped**: Final stage of 3-level unpack had t5,t6 lanes reversed vs C++ Hadamard
2. **Transpose T.S2 pair ordering**: Middle stage pair grouping (t4,t5 vs t2,t3) differed from SSE reference ordering
3. **DC value overwritten**: `g0_abs[7]` spill location conflicted with DC term storage after final abs stage
4. **Wrapper accumulator clobbered**: Kernel's `r10d` usage overwrote the wrapper's loop counter/accumulator
5. **Row advance off by 2×**: Single `lea` instead of double `lea` for Pel stride (2 bytes/Pel) vs byte stride
6. **GAS inline asm push/pop syntax**: Single-line `push rbx\npush rbp` failed to assemble; required separate lines or explicit `.byte` sequences

### Conclusions

- The 8×8 XMM kernel is bit-exact and 1.07x faster but below laptop significance threshold
- The 16×16 approach via 4×8x8 calls is fundamentally flawed — function call overhead dominates
- A competitive 16×16 ASM would need a single-pass YMM kernel processing 8 Pels per operation, comparable to `xCalcHAD16x16_AVX2`
- The fix for non-8x8 dispatch (fallback wrapper delegating to C++ SIMD) is documented and merged separately
