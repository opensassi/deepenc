# dq-asm-minselect-debug

GitHub Issue: https://github.com/opensassi/deepenc/issues/3

## Previous Work

### What Succeeded

- Full DepQuant assembly integration specification and D3 animation
- C++ reference implementation for rdCost computation
- Per-instance m_afpRdCostFunc override pattern proven via SAD ASM work
- NASM build infrastructure

### What Was Tried

- Multiple blend mask strategies (vpblendvb operand order, vpshufd immediates, vpblendd replacement)
- 9 bugs already fixed (PQData offsets, register save/restore, stack corruption, sign extension)
- Round 2 min-select was missing — added valCand2+compare+blend

### What Remains

- **valCand2 (xmm13) gets clobbered** between construction (+0x42c) and blend (+0x46e)
  - At +0x42c: xmm13 = {0x6, 0x5, 0x6, 0x4} (correct)
  - At +0x46e: xmm13 = {0x8000, 0x8000, 0x8000, 0x8000} (garbage)
  - Only instruction between: `vmovdqu xmm4, [rip + .L_idxCand2_asm]` (loads xmm4, not xmm13)
  - Suspect: `vpcmpgtq` for round 2 or `vpblendw`/`vpshufd` for chng2 mask accidentally writes xmm13
- chng2 mask (xmm0) at blend is garbage instead of all-0s/all-1s

### Key Technical Details

```
Function: vvenc_dq_checkAllRdCosts_avx2
Offset map: 0x000 push r12 → 0x0d9 cffBits → 0x18f valCand1 → 0x3d4 valBest init
            → 0x407 Round 1 blend → 0x42c valCand2 load → 0x46e pack → 0x49e ret
```

- 10 bugs already fixed (offset 24→40, 12→24, 40→56; rbx/r12-r15 save/restore; stack add rsp,64; mov ebx vs rbx; vpmovzxwd/wdq/sxdq; round 2 min-select; redundant vmovdqu)
- Microbench at `.profiler/asm-optimizer/microbenchmarks/dq-checkAllRdCosts/`
- ASM in `source/Lib/CommonLib/x86/asm-dq-checkAllRdCosts.cpp`
