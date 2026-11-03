; Game > Benchmark CPU's loops: each runs n turns of eight instructions of one kind, and the two
; that count the turns, so that i_asmos.c can time what one kind of instruction costs in the
; emulator, the cost every optimization of this port is weighed against. buf is 64 KB of scratch.
;
; void bench_<kind>(long n, void *buf)      rdi = n, rsi = buf
; Arguments follow the System V calling convention.

    default rel

    section .text

    global bench_int
    global bench_mul
    global bench_div
    global bench_load
    global bench_store
    global bench_scatter
    global bench_branch
    global bench_call
    global bench_sse
    global bench_avx
    global bench_pextrb
    global bench_gather
    global bench_rep
    global bench_movsb
    global bench_movsq
    global bench_stosd
    global bench_avxcopy

; add and xor between registers
bench_int:
    xor eax, eax
    mov ecx, 3
    mov edx, 5
.turn:
    add eax, ecx
    xor edx, eax
    add ecx, edx
    xor eax, 7
    add edx, 1
    xor ecx, eax
    add eax, edx
    xor edx, 9
    dec rdi
    jnz .turn
    ret

; 32-bit multiplications, as fixed point is
bench_mul:
    mov eax, 3
    mov ecx, 5
    mov edx, 7
.turn:
    imul eax, ecx
    imul ecx, edx
    imul edx, eax
    imul eax, 13
    imul ecx, 17
    imul edx, ecx
    imul eax, edx
    imul ecx, eax
    dec rdi
    jnz .turn
    ret

; 32-bit divisions, as 0xffffffff / scale is
bench_div:
    mov r8, rdi
    mov ecx, 7
.turn:
    mov eax, -1
    xor edx, edx
    div ecx
    mov eax, -1
    xor edx, edx
    div ecx
    mov eax, -1
    div ecx
    dec r8
    jnz .turn
    ret

; loads from one cache line's neighbourhood
bench_load:
    xor eax, eax
.turn:
    mov eax, [rsi]
    mov ecx, [rsi + 4]
    mov edx, [rsi + 8]
    mov r8d, [rsi + 12]
    mov eax, [rsi + 64]
    mov ecx, [rsi + 68]
    mov edx, [rsi + 128]
    mov r8d, [rsi + 132]
    dec rdi
    jnz .turn
    ret

; stores to the same
bench_store:
    xor eax, eax
.turn:
    mov [rsi], eax
    mov [rsi + 4], eax
    mov [rsi + 8], eax
    mov [rsi + 12], eax
    mov [rsi + 64], eax
    mov [rsi + 68], eax
    mov [rsi + 128], eax
    mov [rsi + 132], eax
    dec rdi
    jnz .turn
    ret

; byte stores a row of DOOM's picture apart, as a column is drawn
bench_scatter:
    xor eax, eax
.turn:
    mov [rsi], al
    mov [rsi + 320], al
    mov [rsi + 640], al
    mov [rsi + 960], al
    mov [rsi + 1280], al
    mov [rsi + 1600], al
    mov [rsi + 1920], al
    mov [rsi + 2240], al
    dec rdi
    jnz .turn
    ret

; compares and conditional jumps, half taken
bench_branch:
    xor eax, eax
.turn:
    cmp eax, 1
    je .a
.a:
    cmp eax, 0
    jne .b
.b:
    cmp eax, 1
    je .c
.c:
    cmp eax, 0
    jne .d
.d:
    dec rdi
    jnz .turn
    ret

; calls and returns: four of each a turn
bench_call:
.turn:
    call .leaf
    call .leaf
    call .leaf
    call .leaf
    dec rdi
    jnz .turn
    ret
.leaf:
    ret

; SSE integer additions
bench_sse:
    pxor xmm0, xmm0
    pxor xmm1, xmm1
.turn:
    paddd xmm0, xmm1
    paddd xmm1, xmm0
    paddd xmm2, xmm1
    paddd xmm3, xmm2
    paddd xmm0, xmm3
    paddd xmm1, xmm0
    paddd xmm2, xmm1
    paddd xmm3, xmm2
    dec rdi
    jnz .turn
    ret

; AVX2 integer additions, 256 bits wide
bench_avx:
    vpxor ymm0, ymm0, ymm0
    vpxor ymm1, ymm1, ymm1
.turn:
    vpaddd ymm0, ymm0, ymm1
    vpaddd ymm1, ymm1, ymm0
    vpaddd ymm2, ymm2, ymm1
    vpaddd ymm3, ymm3, ymm2
    vpaddd ymm0, ymm0, ymm3
    vpaddd ymm1, ymm1, ymm0
    vpaddd ymm2, ymm2, ymm1
    vpaddd ymm3, ymm3, ymm2
    dec rdi
    jnz .turn
    vzeroupper
    ret

; byte stores out of a vector register, as doom_column does
bench_pextrb:
    vpxor xmm0, xmm0, xmm0
.turn:
    vpextrb byte [rsi], xmm0, 0
    vpextrb byte [rsi + 320], xmm0, 4
    vpextrb byte [rsi + 640], xmm0, 8
    vpextrb byte [rsi + 960], xmm0, 12
    vpextrb byte [rsi + 1280], xmm0, 1
    vpextrb byte [rsi + 1600], xmm0, 5
    vpextrb byte [rsi + 1920], xmm0, 9
    vpextrb byte [rsi + 2240], xmm0, 13
    dec rdi
    jnz .turn
    vzeroupper
    ret

; eight-element gathers, each with the mask it needs: four of each a turn
bench_gather:
    vpcmpeqd ymm2, ymm2, ymm2
    vpsrld ymm2, ymm2, 29           ; the indices: 7 in every lane
.turn:
    vpcmpeqd ymm1, ymm1, ymm1
    vpgatherdd ymm3, [rsi + ymm2*4], ymm1
    vpcmpeqd ymm1, ymm1, ymm1
    vpgatherdd ymm4, [rsi + ymm2*4], ymm1
    vpcmpeqd ymm1, ymm1, ymm1
    vpgatherdd ymm3, [rsi + ymm2*4], ymm1
    vpcmpeqd ymm1, ymm1, ymm1
    vpgatherdd ymm4, [rsi + ymm2*4], ymm1
    dec rdi
    jnz .turn
    vzeroupper
    ret

; rep movsd of 2560 bytes, a doubled row of the window, then back: two copies a turn
bench_rep:
    mov r8, rdi
    mov r9, rsi
.turn:
    mov rdi, r9
    lea rsi, [r9 + 32768]
    mov ecx, 640
    rep movsd
    lea rdi, [r9 + 32768]
    mov rsi, r9
    mov ecx, 640
    rep movsd
    dec r8
    jnz .turn
    ret

; the same 2560 bytes twice a turn, a byte at a time
bench_movsb:
    mov r8, rdi
    mov r9, rsi
.turn:
    mov rdi, r9
    lea rsi, [r9 + 32768]
    mov ecx, 2560
    rep movsb
    lea rdi, [r9 + 32768]
    mov rsi, r9
    mov ecx, 2560
    rep movsb
    dec r8
    jnz .turn
    ret

; and eight bytes at a time
bench_movsq:
    mov r8, rdi
    mov r9, rsi
.turn:
    mov rdi, r9
    lea rsi, [r9 + 32768]
    mov ecx, 320
    rep movsq
    lea rdi, [r9 + 32768]
    mov rsi, r9
    mov ecx, 320
    rep movsq
    dec r8
    jnz .turn
    ret

; 2560 bytes filled twice a turn, four bytes at a time
bench_stosd:
    mov r8, rdi
    mov r9, rsi
    xor eax, eax
.turn:
    mov rdi, r9
    mov ecx, 640
    rep stosd
    lea rdi, [r9 + 32768]
    mov ecx, 640
    rep stosd
    dec r8
    jnz .turn
    ret

; the same 2560 bytes twice a turn through ymm registers, 128 bytes a turn of the inner loop
bench_avxcopy:
    mov r8, rdi
.turn:
    lea rdi, [rsi + 32768]
    mov rdx, rsi
    mov ecx, 20
.inner:
    vmovdqu ymm0, [rdx]
    vmovdqu ymm1, [rdx + 32]
    vmovdqu ymm2, [rdx + 64]
    vmovdqu ymm3, [rdx + 96]
    vmovdqu [rdi], ymm0
    vmovdqu [rdi + 32], ymm1
    vmovdqu [rdi + 64], ymm2
    vmovdqu [rdi + 96], ymm3
    add rdx, 128
    add rdi, 128
    dec ecx
    jnz .inner
    lea rdx, [rsi + 32768]
    mov rdi, rsi
    mov ecx, 20
.back:
    vmovdqu ymm0, [rdx]
    vmovdqu ymm1, [rdx + 32]
    vmovdqu ymm2, [rdx + 64]
    vmovdqu ymm3, [rdx + 96]
    vmovdqu [rdi], ymm0
    vmovdqu [rdi + 32], ymm1
    vmovdqu [rdi + 64], ymm2
    vmovdqu [rdi + 96], ymm3
    add rdx, 128
    add rdi, 128
    dec ecx
    jnz .back
    dec r8
    jnz .turn
    vzeroupper
    ret
