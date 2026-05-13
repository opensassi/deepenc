// Embedded GAS assembly for DQIntern::State<AVX2>::checkAllRdCosts()
// Compiled inline via GCC asm() to avoid NASM/CMake ASM dependencies.

asm(
".intel_syntax noprefix\n"
".globl vvenc_dq_checkAllRdCosts_avx2\n"
".type vvenc_dq_checkAllRdCosts_avx2, @function\n"
"vvenc_dq_checkAllRdCosts_avx2:\n"
"    push r12\n"
"    push r13\n"
"    push r14\n"
"    push r15\n"
"    push rbx\n"
"    push rbp\n"
"    mov rbp, rsp\n"
"    sub rsp, 64\n"
"    vzeroupper\n"

// Step 1: Load rdCost and compute Z/A/B
"    vmovdqu xmm0, [rcx + 0]\n"
"    vmovdqu xmm1, [rcx + 16]\n"
"    vpunpcklqdq xmm2, xmm0, xmm1\n"
"    vpunpckhqdq xmm3, xmm0, xmm1\n"

// B path: deltaDist from pq[2], pq[1]
"    mov rax, [rsi + 40]\n"
"    mov r8,  [rsi + 24]\n"
"    vmovq xmm4, rax\n"
"    vpinsrq xmm4, xmm4, r8, 1\n"
"    vpaddq xmm5, xmm3, xmm4\n"
"    vpaddq xmm6, xmm2, xmm4\n"

// A path: deltaDist from pq[0], pq[3]
"    mov rax, [rsi + 8]\n"
"    mov r8,  [rsi + 56]\n"
"    vmovq xmm4, rax\n"
"    vpinsrq xmm4, xmm4, r8, 1\n"
"    vpaddq xmm7, xmm2, xmm4\n"
"    vpaddq xmm8, xmm3, xmm4\n"

// Step 2: Load sigBits
"    mov rax, [rcx + 384]\n"
"    mov r8,  [rcx + 392]\n"
"    mov r9,  [rcx + 400]\n"
"    mov r10, [rcx + 408]\n"
"    movzx ebx, byte ptr [rcx + 264]\n"
"    movzx r11d, byte ptr [rcx + 265]\n"
"    movzx r12d, byte ptr [rcx + 266]\n"
"    movzx r13d, byte ptr [rcx + 267]\n"
"    vmovq xmm9, [rax + rbx*8]\n"
"    vmovq xmm10, [r9 + r12*8]\n"
"    vpunpcklqdq xmm9, xmm9, xmm10\n"
"    vmovq xmm10, [r8 + r11*8]\n"
"    vmovq xmm11, [r10 + r13*8]\n"
"    vpunpcklqdq xmm10, xmm10, xmm11\n"

// Reorder sigBits
"    vpshufd xmm11, xmm9, 0x88\n"
"    vpshufd xmm12, xmm9, 0xDD\n"
"    vpunpcklqdq xmm9, xmm11, xmm12\n"
"    vpshufd xmm11, xmm10, 0x88\n"
"    vpshufd xmm12, xmm10, 0xDD\n"
"    vpunpcklqdq xmm13, xmm11, xmm12\n"

// Step 3: cffBits gather — precomputed base ptrs, memory-folded loads
"    movzx ebx, byte ptr [rcx + 268]\n"    // ctx.cff[0]
"    movzx r11d, byte ptr [rcx + 269]\n"   // ctx.cff[1]
"    movzx r8d,  byte ptr [rcx + 270]\n"   // ctx.cff[2]
"    movzx r9d,  byte ptr [rcx + 271]\n"   // ctx.cff[3]
"    mov rax, [rcx + 416]\n"                // gtxFracBits base ptr

// Compute base0..3 = gtx + ctx[N]*24 via lea chain: ctx*3 then *8
"    lea rbx, [rbx + rbx*2]\n"
"    lea rbp, [rax + rbx*8]\n"             // base0
"    lea r11, [r11 + r11*2]\n"
"    lea r13, [rax + r11*8]\n"             // base1
"    lea r8, [r8 + r8*2]\n"
"    lea r12, [rax + r8*8]\n"              // base2
"    lea r9, [r9 + r9*2]\n"
"    lea r14, [rax + r9*8]\n"              // base3

// Load absLevel indices
"    movsx r15d, word ptr [rsi + 16]\n"    // pq[1].absLevel
"    movsx ebx, word ptr [rsi + 32]\n"     // pq[2].absLevel
"    movsx r8d, word ptr [rsi + 0]\n"      // pq[0].absLevel
"    movsx r9d, word ptr [rsi + 48]\n"     // pq[3].absLevel

// B path: [ctx[1]][pq2], [ctx[3]][pq1], [ctx[0]][pq2], [ctx[2]][pq1]
"    vmovd xmm14, [r13 + rbx*4]\n"
"    vpinsrd xmm14, xmm14, [r14 + r15*4], 1\n"
"    vpinsrd xmm14, xmm14, [rbp + rbx*4], 2\n"
"    vpinsrd xmm14, xmm14, [r12 + r15*4], 3\n"
"    vpmovsxdq ymm14, xmm14\n"
"    vextracti128 xmm15, ymm14, 1\n"
"    vpaddq xmm5, xmm5, xmm14\n"
"    vpaddq xmm6, xmm6, xmm15\n"

// A path: [ctx[0]][pq0], [ctx[2]][pq3], [ctx[1]][pq0], [ctx[3]][pq3]
"    vmovd xmm14, [rbp + r8*4]\n"
"    vpinsrd xmm14, xmm14, [r12 + r9*4], 1\n"
"    vpinsrd xmm14, xmm14, [r13 + r8*4], 2\n"
"    vpinsrd xmm14, xmm14, [r14 + r9*4], 3\n"
"    vpmovsxdq ymm14, xmm14\n"
"    vextracti128 xmm15, ymm14, 1\n"
"    vpaddq xmm7, xmm7, xmm14\n"
"    vpaddq xmm8, xmm8, xmm15\n"

// Step 4: spt dispatch
"    cmp edi, 0\n"
"    je .L_iscsbb_asm\n"
"    cmp edi, 1\n"
"    je .L_socsbb_asm\n"
"    jmp .L_eocsbb_asm\n"

".L_iscsbb_asm:\n"
"    vpmovzxdq ymm14, xmm9\n"
"    vextracti128 xmm15, ymm14, 1\n"
"    vpaddq xmm2, xmm2, xmm14\n"
"    vpmovzxdq ymm14, xmm13\n"
"    vextracti128 xmm0, ymm14, 1\n"
"    vpaddq xmm3, xmm3, xmm14\n"
"    vpaddq xmm7, xmm7, xmm15\n"
"    vpaddq xmm6, xmm6, xmm15\n"
"    vpaddq xmm8, xmm8, xmm0\n"
"    vpaddq xmm5, xmm5, xmm0\n"
"    jmp .L_select_asm\n"

".L_socsbb_asm:\n"
"    vmovdqu xmm14, [rcx + 56]\n"
"    vpshufd xmm14, xmm14, 0xD8\n"
"    vpmovzxdq ymm15, xmm9\n"
"    vextracti128 xmm11, ymm15, 1\n"
"    vpaddq xmm2, xmm2, xmm15\n"
"    vpmovzxdq ymm15, xmm13\n"
"    vextracti128 xmm0, ymm15, 1\n"
"    vpaddq xmm3, xmm3, xmm15\n"
"    vpmovsxdq ymm15, xmm14\n"
"    vextracti128 xmm4, ymm15, 1\n"
"    vpaddq xmm2, xmm2, xmm15\n"
"    vpaddq xmm3, xmm3, xmm4\n"
"    vpaddq xmm7, xmm7, xmm15\n"
"    vpaddq xmm6, xmm6, xmm15\n"
"    vpaddq xmm8, xmm8, xmm4\n"
"    vpaddq xmm5, xmm5, xmm4\n"
"    vpaddq xmm7, xmm7, xmm11\n"
"    vpaddq xmm6, xmm6, xmm11\n"
"    vpaddq xmm8, xmm8, xmm0\n"
"    vpaddq xmm5, xmm5, xmm0\n"
"    jmp .L_select_asm\n"

".L_eocsbb_asm:\n"
"    movzx ebx, byte ptr [rcx + 272]\n"
"    movzx r11d, byte ptr [rcx + 273]\n"
"    movzx r8d,  byte ptr [rcx + 274]\n"
"    movzx r9d,  byte ptr [rcx + 275]\n"
"    vpmovzxdq ymm14, xmm9\n"
"    vextracti128 xmm15, ymm14, 1\n"
"    vpaddq xmm2, xmm2, xmm14\n"
"    vpmovzxdq ymm14, xmm13\n"
"    vextracti128 xmm0, ymm14, 1\n"
"    vpaddq xmm3, xmm3, xmm14\n"
"    mov [rsp], ebx\n"
"    mov [rsp+4], r11d\n"
"    mov [rsp+8], r8d\n"
"    mov [rsp+12], r9d\n"
"    vmovdqu xmm14, [rsp]\n"
"    vpxor xmm1, xmm1, xmm1\n"
"    vpcmpgtb xmm14, xmm14, xmm1\n"
"    vpshufb xmm4, xmm14, [rip + .L_mask_lo_asm]\n"
"    vpshufb xmm1, xmm14, [rip + .L_mask_hi_asm]\n"
"    vpand xmm14, xmm15, xmm4\n"
"    vpaddq xmm7, xmm7, xmm14\n"
"    vpaddq xmm6, xmm6, xmm14\n"
"    vpand xmm14, xmm0, xmm1\n"
"    vpaddq xmm8, xmm8, xmm14\n"
"    vpaddq xmm5, xmm5, xmm14\n"
"    vmovq xmm14, [rip + .L_rdCostInit_asm]\n"
"    vpunpcklqdq xmm14, xmm14, xmm14\n"
"    vpblendvb xmm2, xmm14, xmm2, xmm4\n"
"    vpblendvb xmm3, xmm14, xmm3, xmm1\n"

".L_select_asm:\n"
"    vmovdqa xmm9, xmm2\n"
"    vmovdqa xmm10, xmm6\n"
"    movsx r14d, word ptr [rsi + 32]\n"
"    movsx r15d, word ptr [rsi + 16]\n"
"    vpxor xmm11, xmm11, xmm11\n"
"    vpinsrd xmm11, xmm11, r14d, 2\n"
"    vpinsrd xmm11, xmm11, r15d, 3\n"
"    vmovdqu xmm12, [rip + .L_idxBest_asm]\n"
"    vmovdqu xmm4, [rip + .L_idxCand_asm]\n"
// Round 1: valCand = {pq[0], pq[3], 0, 0}
"    movsx r8d, word ptr [rsi + 0]\n"
"    movsx r9d, word ptr [rsi + 48]\n"
"    vpxor xmm13, xmm13, xmm13\n"
"    vpinsrd xmm13, xmm13, r8d, 0\n"
"    vpinsrd xmm13, xmm13, r9d, 1\n"
// Round 1: compare Z01 > A01, B23 > Z23
"    vpcmpgtq xmm14, xmm9, xmm7\n"
"    vpcmpgtq xmm15, xmm10, xmm3\n"
"    vpblendw xmm0, xmm14, xmm15, 0xCC\n"
"    vpshufd xmm0, xmm0, 0xD8\n"
"    vpblendvb xmm9, xmm9, xmm7, xmm14\n"
"    vpblendvb xmm10, xmm10, xmm3, xmm15\n"
"    vpblendvb xmm11, xmm11, xmm13, xmm0\n"
"    vpblendvb xmm12, xmm12, xmm4, xmm0\n"

// Round 2: compare with remaining B/A candidates
// chng01 = rdBest01 > B01, chng23 = rdBest23 > A23
"    vpcmpgtq xmm14, xmm9, xmm5\n"
"    vpcmpgtq xmm15, xmm10, xmm8\n"
"    vpblendw xmm0, xmm14, xmm15, 0xCC\n"
"    vpshufd xmm0, xmm0, 0xD8\n"

"    vpblendvb xmm9, xmm9, xmm5, xmm14\n"
"    vpblendvb xmm10, xmm10, xmm8, xmm15\n"

// valCand2 = {pq[2], pq[1], pq[0], pq[3]}
"    movsx r14d, word ptr [rsi + 32]\n"
"    movsx r15d, word ptr [rsi + 16]\n"
"    movsx r8d,  word ptr [rsi + 0]\n"
"    movsx r9d,  word ptr [rsi + 48]\n"
"    vmovd xmm13, r14d\n"
"    vpinsrd xmm13, xmm13, r15d, 1\n"
"    vpinsrd xmm13, xmm13, r8d, 2\n"
"    vpinsrd xmm13, xmm13, r9d, 3\n"

// idxCand2 = {1, 3, 1, 3}
"    vmovdqu xmm4, [rip + .L_idxCand2_asm]\n"

// Blend round 2
"    vpblendvb xmm11, xmm11, xmm13, xmm0\n"
"    vpblendvb xmm12, xmm12, xmm4, xmm0\n"

// Store results
"    vpxor xmm0, xmm0, xmm0\n"
"    vpackssdw xmm11, xmm11, xmm0\n"
"    vpackssdw xmm12, xmm12, xmm0\n"
"    vpacksswb xmm12, xmm12, xmm0\n"
"    vmovdqu [rdx + 0], xmm9\n"
"    vmovdqu [rdx + 16], xmm10\n"
"    vmovq [rdx + 32], xmm11\n"
"    vmovd [rdx + 40], xmm12\n"
"    vzeroupper\n"
"    add rsp, 64\n"
"    pop rbp\n"
"    pop rbx\n"
"    pop r15\n"
"    pop r14\n"
"    pop r13\n"
"    pop r12\n"
"    ret\n"

// Read-only data (in .text to avoid section switching issues)
".balign 16\n"
".L_rdCostInit_asm:\n"
"    .quad 0x3FFFFFFFFFFFFFFF\n"
".L_mask_lo_asm:\n"
"    .byte 0,0,0,0,0,0,0,0, 8,8,8,8,8,8,8,8\n"
".L_mask_hi_asm:\n"
"    .byte 4,4,4,4,4,4,4,4, 12,12,12,12,12,12,12,12\n"
".L_idxBest_asm:\n"
"    .int 0, 2, 0, 2\n"
".L_idxCand_asm:\n"
"    .int 0, 2, 1, 3\n"
".L_idxCand2_asm:\n"
"    .int 1, 3, 1, 3\n"
".text\n"
);
