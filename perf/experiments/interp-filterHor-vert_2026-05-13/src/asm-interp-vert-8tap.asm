; AVX2 8-tap vertical luma interpolation filter
; Implements filterVer[0][0][0]: N=8, isFirst=false, isLast=false
; void(const ClpRng&, Pel const*, int, Pel*, int, int, int, TFilterCoeff const*)
;
; ABI: rdi=clpRng, rsi=src, rdx=srcStride, rcx=dst, r8=dstStride,
;      r9=width, [rsp+0]=height, [rsp+8]=coeff
;
; Strategy: process 16 Pels per column group (across all rows).
; Load 8 row-slices of 16 Pels, then produce 16 Pels per output row
; by interleaving row pairs, vpmaddwd with coefficient pairs, accumulating.

default rel

SECTION .text

global vvenc_interp_vert_8tap_avx2
vvenc_interp_vert_8tap_avx2:
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15
    mov rbp, rsp
    vzeroupper

    ; Load params from stack
    mov r15d, [rbp + 56]          ; height (original)
    mov r14d, r9d                 ; width
    mov rax, [rbp + 64]           ; coeff ptr

    ; Load and interleave coefficients
    vpbroadcastw ymm1, [rax + 0]  ; c0
    vpbroadcastw ymm2, [rax + 2]  ; c1
    vpbroadcastw ymm3, [rax + 4]  ; c2
    vpbroadcastw ymm4, [rax + 6]  ; c3
    vpbroadcastw ymm5, [rax + 8]  ; c4
    vpbroadcastw ymm6, [rax + 10] ; c5
    vpbroadcastw ymm7, [rax + 12] ; c6
    vpbroadcastw ymm8, [rax + 14] ; c7

    ; Interleave into pairs
    {vex3} vpunpcklwd ymm9, ymm1, ymm2     ; {c0,c1}
    {vex3} vpunpcklwd ymm10, ymm3, ymm4    ; {c2,c3}
    {vex3} vpunpcklwd ymm11, ymm5, ymm6    ; {c4,c5}
    {vex3} vpunpcklwd ymm12, ymm7, ymm8    ; {c6,c7}

    ; Convert Pel- strides to byte strides
    movsxd rdx, edx               ; rdx = srcStride (Pels)
    movsxd r8, r8d                ; r8  = dstStride (Pels)
    movsxd r14, r14d              ; r14 = width (Pels)
    ; Convert stride from Pel units to byte units
    sal rdx, 1                    ; rdx = srcStride * 2 (byte stride)
    sal r8, 1                     ; r8  = dstStride * 2 (byte stride)
    ; Apply vertical centering: src -= 3 * srcStride_bytes
    lea rbx, [rdx + rdx*2]       ; rbx = 3 * stride_bytes
    sub rsi, rbx                  ; rsi = src - 3*stride

    ; Only handle width >= 16 and width % 16 == 0
    cmp r14d, 16
    jl .L_epilogue
    test r14d, 15
    jnz .L_epilogue

.L_col_loop:
    ; Reload height (may have been decremented by previous column group)
    mov r15d, [rbp + 56]
    ; Save current position in column group
    mov r13, rsi                  ; save src start for this column group
    mov r12, rcx                  ; save dst start for this column group

    ; Load 8 rows (N=8) of 16 Pels each
    ; vsrc[0..7] = ymm0..ymm7. rdx = srcStride bytes.
    lea rbx, [rdx + rdx*2]        ; rbx = stride*3
    lea r9, [rdx + rdx*4]         ; r9  = stride*5
    ; stride*6 = (stride+stride*2)*2 = rbx*2
    lea r10, [rbx + rbx]          ; r10 = stride*6
    lea r11, [rdx + r9]           ; r11 = stride*7 = rdx + stride*5

    {vex3} vmovdqu ymm0, [rsi]           ; row 0
    {vex3} vmovdqu ymm1, [rsi + rdx]     ; row 1
    {vex3} vmovdqu ymm2, [rsi + rdx*2]   ; row 2
    {vex3} vmovdqu ymm3, [rsi + rbx]     ; row 3 = stride*3
    {vex3} vmovdqu ymm4, [rsi + rdx*4]   ; row 4 = stride*4  (rdx*4 is valid!)
    {vex3} vmovdqu ymm5, [rsi + r9]      ; row 5 = stride*5
    {vex3} vmovdqu ymm6, [rsi + r10]     ; row 6 = stride*6
    {vex3} vmovdqu ymm7, [rsi + r11]     ; row 7 = stride*7

    ; r9 = pointer to row 8 (next row to load) = rsi + stride*8
    lea r9, [rsi + rdx*8]

.L_row_loop:
    ; Load the next row into ymm8 (replacing the oldest)
    {vex3} vmovdqu ymm8, [r9]

    ; Process 4 coefficient pairs, splitting into low/high
    ; Each vpunpcklwd(ymmX, ymmY) interleaves low 8 words -> 8 dwords
    ; Each vpunpckhwd(ymmX, ymmY) interleaves high 8 words -> 8 dwords

    ; Pair 0: {c0,c1}
    {vex3} vpunpcklwd ymm13, ymm0, ymm1
    {vex3} vpunpckhwd ymm14, ymm0, ymm1
    {vex3} vpmaddwd ymm13, ymm9, ymm13
    {vex3} vpmaddwd ymm14, ymm9, ymm14

    ; Pair 1: {c2,c3}
    {vex3} vpunpcklwd ymm15, ymm2, ymm3
    {vex3} vpunpckhwd ymm0, ymm2, ymm3           ; ymm0 no longer needed as src row
    {vex3} vpmaddwd ymm15, ymm10, ymm15
    {vex3} vpmaddwd ymm0, ymm10, ymm0

    {vex3} vpaddd ymm13, ymm13, ymm15
    {vex3} vpaddd ymm14, ymm14, ymm0

    ; Pair 2: {c4,c5}
    {vex3} vpunpcklwd ymm15, ymm4, ymm5
    {vex3} vpunpckhwd ymm0, ymm4, ymm5
    {vex3} vpmaddwd ymm15, ymm11, ymm15
    {vex3} vpmaddwd ymm0, ymm11, ymm0

    {vex3} vpaddd ymm13, ymm13, ymm15
    {vex3} vpaddd ymm14, ymm14, ymm0

    ; Pair 3: {c6,c7}
    {vex3} vpunpcklwd ymm15, ymm6, ymm7
    {vex3} vpunpckhwd ymm0, ymm6, ymm7
    {vex3} vpmaddwd ymm15, ymm12, ymm15
    {vex3} vpmaddwd ymm0, ymm12, ymm0

    {vex3} vpaddd ymm13, ymm13, ymm15           ; ymm13 = full sum for low 8 Pels
    {vex3} vpaddd ymm14, ymm14, ymm0            ; ymm14 = full sum for high 8 Pels

    ; Shift by 6
    {vex3} vpsrad ymm13, ymm13, 6
    {vex3} vpsrad ymm14, ymm14, 6

    ; Pack and store
    {vex3} vpackssdw ymm13, ymm13, ymm14   ; ymm13=suma, ymm14=sumb
    {vex3} vmovdqu [rcx], ymm13

    ; Shift the row buffer: vsrc[i] = vsrc[i+1], vsrc[7] = vsrc[8]
    ; ymm0 is scratch (contains pair3 high result), reload rows
    {vex3} vmovdqu ymm0, ymm1               ; vsrc[0] = old vsrc[1]
    {vex3} vmovdqu ymm1, ymm2
    {vex3} vmovdqu ymm2, ymm3
    {vex3} vmovdqu ymm3, ymm4
    {vex3} vmovdqu ymm4, ymm5
    {vex3} vmovdqu ymm5, ymm6
    {vex3} vmovdqu ymm6, ymm7
    {vex3} vmovdqu ymm7, ymm8               ; vsrc[7] = just-loaded row

    ; Advance pointers
    add rsi, rdx                    ; src += srcStride
    add rcx, r8                     ; dst += dstStride
    add r9, rdx                    ; next load ptr += srcStride

    dec r15d
    jnz .L_row_loop

    ; Move to next column group (16 Pels)
    mov rsi, r13                    ; restore src start
    mov rcx, r12                    ; restore dst start
    add rsi, 32                     ; advance src by 16 Pels (= 32 bytes)
    add rcx, 32                     ; advance dst by 16 Pels
    ; Reset next-row pointer for new column group
    lea r9, [rsi + rdx*8]          ; r9 = new col start + 8*stride (row for next load)
    sub r14d, 16
    jnz .L_col_loop

.L_epilogue:
    vzeroupper
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    ret
