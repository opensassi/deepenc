; AVX2 8-tap horizontal luma interpolation filter
; Implements filterHor[0][0][0]: N=8, isFirst=false, isLast=false
; void(const ClpRng&, Pel const*, int, Pel*, int, int, int, TFilterCoeff const*)
;
; ABI (System V AMD64):
;   rdi = &clpRng, rsi = src, rdx = srcStride, rcx = dst,
;   r8 = dstStride, r9 = width, [rsp+0] = height, [rsp+8] = coeff
;
; Strategy: process 16 Pels per iteration using vpmaddwd with 4 coefficient pairs.
; Even outputs from byte offsets 0,+4,+8,+12; Odd outputs from +2,+6,+10,+14.
; Pack with vpackssdw, interleave via vpshufb.

default rel

SECTION .text

global vvenc_interp_horiz_8tap_avx2
vvenc_interp_horiz_8tap_avx2:
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15
    mov rbp, rsp
    vzeroupper

    ; Load coeff ptr from stack: [rbp+48] = return_addr, +8=height, +16=coeff
    mov rax, [rbp + 64]

    ; Load 8 coefficients via broadcast
    vpbroadcastw ymm4, [rax + 0]     ; c0
    vpbroadcastw ymm5, [rax + 2]     ; c1
    vpbroadcastw ymm6, [rax + 4]     ; c2
    vpbroadcastw ymm7, [rax + 6]     ; c3
    vpbroadcastw ymm8, [rax + 8]     ; c4
    vpbroadcastw ymm9, [rax + 10]    ; c5
    vpbroadcastw ymm10, [rax + 12]   ; c6
    vpbroadcastw ymm11, [rax + 14]   ; c7

    ; Interleave into coefficient pairs for vpmaddwd
    {vex3} vpunpcklwd ymm4, ymm4, ymm5     ; {c0,c1} 16 words
    {vex3} vpunpcklwd ymm6, ymm6, ymm7     ; {c2,c3}
    {vex3} vpunpcklwd ymm8, ymm8, ymm9     ; {c4,c5}
    {vex3} vpunpcklwd ymm10, ymm10, ymm11  ; {c6,c7}

    ; Interleave mask for vpshufb
    ; After vpackssdw(odd, even): [o0,o1,o2,o3, e0,e1,e2,e3] per lane
    ; Want: [e0,o0, e1,o1, e2,o2, e3,o3, ...]
    ; Mask selects: byte 8,9 then byte 0,1 then byte 10,11 then byte 2,3 ...
    jmp .L_after_mask
align 32
.L_interleave_mask:
    db 8,9, 0,1, 10,11, 2,3, 12,13, 4,5, 14,15, 6,7
    db 8,9, 0,1, 10,11, 2,3, 12,13, 4,5, 14,15, 6,7
.L_after_mask:
    {vex3} vmovdqu ymm12, [rel .L_interleave_mask]

    ; Load height and width parameters
    mov r15d, [rbp + 56]          ; height
    mov r14d, r9d                 ; width

    ; Adjust src: subtract 6 bytes (=3 Pels) for filter centering
    lea rsi, [rsi - 6]

    ; Sign-extend strides
    movsxd rdx, edx
    movsxd r8, r8d
    movsxd r14, r14d

    ; Convert stride from Pel units to bytes (sizeof(Pel) = 2)
    lea r12, [rdx + rdx]     ; r12 = srcStride * 2 (byte row stride for src)
    lea r8, [r8 + r8]        ; r8  = dstStride * 2 (byte row stride for dst)

    ; Only handle width >= 16 and width % 16 == 0
    cmp r14d, 16
    jl .L_epilogue
    test r14d, 15
    jnz .L_epilogue

.L_row_loop:
    xor r10d, r10d                  ; column offset in Pels

.L_col_loop:
    ; Even group: load 4 windows for outputs at stride 2
    {vex3} vmovdqu ymm0, [rsi + r10*2]         ; src[col-3..col+12]
    {vex3} vmovdqu ymm1, [rsi + r10*2 + 4]     ; src[col-1..col+14]
    {vex3} vmovdqu ymm2, [rsi + r10*2 + 8]     ; src[col+1..col+16]
    {vex3} vmovdqu ymm3, [rsi + r10*2 + 12]    ; src[col+3..col+18]

    {vex3} vpmaddwd ymm0, ymm4, ymm0           ; {c0,c1} * src
    {vex3} vpmaddwd ymm1, ymm6, ymm1           ; {c2,c3}
    {vex3} vpmaddwd ymm2, ymm8, ymm2           ; {c4,c5}
    {vex3} vpmaddwd ymm3, ymm10, ymm3          ; {c6,c7}

    {vex3} vpaddd ymm0, ymm0, ymm1
    {vex3} vpaddd ymm2, ymm2, ymm3
    {vex3} vpaddd ymm14, ymm0, ymm2            ; ymm14 = even results (8 dwords)

    ; Odd group: load 4 windows shifted by 1 Pel
    {vex3} vmovdqu ymm0, [rsi + r10*2 + 2]     ; src[col-2..col+13]
    {vex3} vmovdqu ymm1, [rsi + r10*2 + 6]     ; src[col..col+15]
    {vex3} vmovdqu ymm2, [rsi + r10*2 + 10]    ; src[col+2..col+17]
    {vex3} vmovdqu ymm3, [rsi + r10*2 + 14]    ; src[col+4..col+19]

    {vex3} vpmaddwd ymm0, ymm4, ymm0
    {vex3} vpmaddwd ymm1, ymm6, ymm1
    {vex3} vpmaddwd ymm2, ymm8, ymm2
    {vex3} vpmaddwd ymm3, ymm10, ymm3

    {vex3} vpaddd ymm0, ymm0, ymm1
    {vex3} vpaddd ymm2, ymm2, ymm3
    {vex3} vpaddd ymm15, ymm0, ymm2            ; ymm15 = odd results (8 dwords)

    ; Shift by 6 (divide by 64)
    {vex3} vpsrad ymm14, ymm14, 6
    {vex3} vpsrad ymm15, ymm15, 6

    ; Pack and interleave
    {vex3} vpackssdw ymm0, ymm15, ymm14
    {vex3} vpshufb ymm0, ymm0, ymm12

    ; Store 16 Pels
    vmovdqu [rcx + r10*2], ymm0

    add r10d, 16
    cmp r10d, r14d
    jl .L_col_loop

    ; Advance to next row
    add rsi, r12
    add rcx, r8
    dec r15d
    jnz .L_row_loop

.L_epilogue:
    vzeroupper
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    ret
