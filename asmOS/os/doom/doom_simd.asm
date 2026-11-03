; DOOM's pixel loops in AVX2, eight pixels at a time. The emulator's time goes by instructions, not
; by the work each does, and in C a pixel of a wall is twelve instructions and a pixel of a floor
; fifteen: here a vpgatherdd fetches eight texels, or eight colours, in one. The editor's C compiler
; makes no AVX, so these are written by hand, as os/blit.asm is; r_draw.c and i_asmos.c call them
; while Game > Fast Drawing is on.
;
; A gather reads four bytes for each one wanted, so it may read up to three bytes past a texture or a
; colour map: those are in DOOM's own memory, which goes on past them.
;
; Arguments follow the System V calling convention: rdi, rsi, rdx, rcx, r8, r9.

    default rel

    section .rodata
    align 32
lanes:      dd 0, 1, 2, 3, 4, 5, 6, 7
low_bytes:  db 0, 4, 8, 12, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80
            db 0, 4, 8, 12, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80
twin_bytes: db 0, 0, 4, 4, 8, 8, 12, 12, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80
            db 0, 0, 4, 4, 8, 8, 12, 12, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80
twin_low:   dd 0, 0, 1, 1, 2, 2, 3, 3     ; vpermd's picks: each of eight colours twice
twin_high:  dd 4, 4, 5, 5, 6, 6, 7, 7
mask_127:   dd 127
twice:      dd 0x101                ; a byte times this is the byte twice
mask_255:   dd 255
mask_4032:  dd 4032
silence:    dd 128                  ; an 8-bit sample's middle

    section .text

    global doom_column
    global doom_span
    global doom_column2
    global doom_span2
    global doom_palette_row
    global doom_palette_row2
    global doom_row_changed
    global doom_mix_add
    global doom_mix_out

; void doom_column(byte *dest, int count, fixed_t frac, fixed_t step, const byte *source,
;                  const byte *colormap)
; R_DrawColumn's loop: count pixels down from dest, one row of 320 apart, each
; colormap[source[(frac >> 16) & 127]], frac going up by step.
doom_column:
    cmp esi, 8
    jl .tail
    vmovd xmm0, edx
    vpbroadcastd ymm0, xmm0
    vmovd xmm1, ecx
    vpbroadcastd ymm1, xmm1
    vpmulld ymm2, ymm1, [lanes]
    vpaddd ymm0, ymm0, ymm2         ; each lane's frac
    vpslld ymm1, ymm1, 3            ; eight steps
    vpbroadcastd ymm3, [mask_127]
    vpbroadcastd ymm4, [mask_255]
    sub esi, 8                      ; the pixels after these eight: the loop ends below zero
.eight:
    vpsrld ymm5, ymm0, 16
    vpand ymm5, ymm5, ymm3
    vpcmpeqd ymm6, ymm6, ymm6
    vpgatherdd ymm7, [r8 + ymm5*1], ymm6
    vpand ymm7, ymm7, ymm4          ; the texels
    vpcmpeqd ymm6, ymm6, ymm6
    vpgatherdd ymm5, [r9 + ymm7*1], ymm6    ; their colours, in each lane's low byte
    vpextrb byte [rdi], xmm5, 0
    vpextrb byte [rdi + 320], xmm5, 4
    vpextrb byte [rdi + 640], xmm5, 8
    vpextrb byte [rdi + 960], xmm5, 12
    vextracti128 xmm5, ymm5, 1
    vpextrb byte [rdi + 1280], xmm5, 0
    vpextrb byte [rdi + 1600], xmm5, 4
    vpextrb byte [rdi + 1920], xmm5, 8
    vpextrb byte [rdi + 2240], xmm5, 12
    vpaddd ymm0, ymm0, ymm1
    add rdi, 2560
    sub esi, 8
    jns .eight
    add esi, 8                      ; the 0 to 7 left
    vmovd edx, xmm0                 ; the next pixel's frac
    vzeroupper
.tail:
    test esi, esi
    jle .done
.one:
    mov eax, edx
    shr eax, 16
    and eax, 127
    movzx eax, byte [r8 + rax]
    movzx eax, byte [r9 + rax]
    mov [rdi], al
    add rdi, 320
    add edx, ecx
    dec esi
    jnz .one
.done:
    ret

; void doom_span(byte *dest, int count, unsigned position, unsigned step, const byte *source,
;                const byte *colormap)
; R_DrawSpan's loop: count pixels rightwards from dest, each colormap[source[spot]] of a 64 by 64
; flat, spot ((position >> 4) & 4032) | (position >> 26), position going up by step.
doom_span:
    cmp esi, 8
    jl .tail
    vmovd xmm0, edx
    vpbroadcastd ymm0, xmm0
    vmovd xmm1, ecx
    vpbroadcastd ymm1, xmm1
    vpmulld ymm2, ymm1, [lanes]
    vpaddd ymm0, ymm0, ymm2         ; each lane's position
    vpslld ymm1, ymm1, 3
    vpbroadcastd ymm3, [mask_4032]
    vpbroadcastd ymm4, [mask_255]
    vmovdqu ymm8, [low_bytes]
    sub esi, 8
.eight:
    vpsrld ymm5, ymm0, 4
    vpand ymm5, ymm5, ymm3
    vpsrld ymm6, ymm0, 26
    vpor ymm5, ymm5, ymm6           ; the spots
    vpcmpeqd ymm6, ymm6, ymm6
    vpgatherdd ymm7, [r8 + ymm5*1], ymm6
    vpand ymm7, ymm7, ymm4
    vpcmpeqd ymm6, ymm6, ymm6
    vpgatherdd ymm5, [r9 + ymm7*1], ymm6
    vpshufb ymm5, ymm5, ymm8        ; each half's four colours in its low four bytes
    vextracti128 xmm6, ymm5, 1
    vpunpckldq xmm5, xmm5, xmm6
    vmovq qword [rdi], xmm5
    vpaddd ymm0, ymm0, ymm1
    add rdi, 8
    sub esi, 8
    jns .eight
    add esi, 8
    vmovd edx, xmm0
    vzeroupper
.tail:
    test esi, esi
    jle .done
.one:
    mov eax, edx
    shr eax, 4
    and eax, 4032
    mov r10d, edx
    shr r10d, 26
    or eax, r10d
    movzx eax, byte [r8 + rax]
    movzx eax, byte [r9 + rax]
    mov [rdi], al
    inc rdi
    add edx, ecx
    dec esi
    jnz .one
.done:
    ret

; void doom_column2(byte *dest, int count, fixed_t frac, fixed_t step, const byte *source,
;                   const byte *colormap)
; R_DrawColumnLow's loop: doom_column's, but each pixel two wide, dest and dest + 1.
doom_column2:
    cmp esi, 8
    jl .tail
    vmovd xmm0, edx
    vpbroadcastd ymm0, xmm0
    vmovd xmm1, ecx
    vpbroadcastd ymm1, xmm1
    vpmulld ymm2, ymm1, [lanes]
    vpaddd ymm0, ymm0, ymm2
    vpslld ymm1, ymm1, 3
    vpbroadcastd ymm3, [mask_127]
    vpbroadcastd ymm4, [mask_255]
    vpbroadcastd ymm8, [twice]
    sub esi, 8
.eight:
    vpsrld ymm5, ymm0, 16
    vpand ymm5, ymm5, ymm3
    vpcmpeqd ymm6, ymm6, ymm6
    vpgatherdd ymm7, [r8 + ymm5*1], ymm6
    vpand ymm7, ymm7, ymm4
    vpcmpeqd ymm6, ymm6, ymm6
    vpgatherdd ymm5, [r9 + ymm7*1], ymm6
    vpand ymm5, ymm5, ymm4
    vpmulld ymm5, ymm5, ymm8        ; each colour twice, in each lane's low word
    vpextrw word [rdi], xmm5, 0
    vpextrw word [rdi + 320], xmm5, 2
    vpextrw word [rdi + 640], xmm5, 4
    vpextrw word [rdi + 960], xmm5, 6
    vextracti128 xmm5, ymm5, 1
    vpextrw word [rdi + 1280], xmm5, 0
    vpextrw word [rdi + 1600], xmm5, 2
    vpextrw word [rdi + 1920], xmm5, 4
    vpextrw word [rdi + 2240], xmm5, 6
    vpaddd ymm0, ymm0, ymm1
    add rdi, 2560
    sub esi, 8
    jns .eight
    add esi, 8
    vmovd edx, xmm0
    vzeroupper
.tail:
    test esi, esi
    jle .done
.one:
    mov eax, edx
    shr eax, 16
    and eax, 127
    movzx eax, byte [r8 + rax]
    movzx eax, byte [r9 + rax]
    imul eax, eax, 0x101
    mov [rdi], ax
    add rdi, 320
    add edx, ecx
    dec esi
    jnz .one
.done:
    ret

; void doom_span2(byte *dest, int count, unsigned position, unsigned step, const byte *source,
;                 const byte *colormap)
; R_DrawSpanLow's loop: doom_span's, but each pixel two wide, so 2 * count bytes from dest.
doom_span2:
    cmp esi, 8
    jl .tail
    vmovd xmm0, edx
    vpbroadcastd ymm0, xmm0
    vmovd xmm1, ecx
    vpbroadcastd ymm1, xmm1
    vpmulld ymm2, ymm1, [lanes]
    vpaddd ymm0, ymm0, ymm2
    vpslld ymm1, ymm1, 3
    vpbroadcastd ymm3, [mask_4032]
    vpbroadcastd ymm4, [mask_255]
    vmovdqu ymm8, [twin_bytes]
    sub esi, 8
.eight:
    vpsrld ymm5, ymm0, 4
    vpand ymm5, ymm5, ymm3
    vpsrld ymm6, ymm0, 26
    vpor ymm5, ymm5, ymm6
    vpcmpeqd ymm6, ymm6, ymm6
    vpgatherdd ymm7, [r8 + ymm5*1], ymm6
    vpand ymm7, ymm7, ymm4
    vpcmpeqd ymm6, ymm6, ymm6
    vpgatherdd ymm5, [r9 + ymm7*1], ymm6
    vpshufb ymm5, ymm5, ymm8        ; each half's four colours, each twice, in its low eight bytes
    vextracti128 xmm6, ymm5, 1
    vpunpcklqdq xmm5, xmm5, xmm6
    vmovdqu [rdi], xmm5
    vpaddd ymm0, ymm0, ymm1
    add rdi, 16
    sub esi, 8
    jns .eight
    add esi, 8
    vmovd edx, xmm0
    vzeroupper
.tail:
    test esi, esi
    jle .done
.one:
    mov eax, edx
    shr eax, 4
    and eax, 4032
    mov r10d, edx
    shr r10d, 26
    or eax, r10d
    movzx eax, byte [r8 + rax]
    movzx eax, byte [r9 + rax]
    imul eax, eax, 0x101
    mov [rdi], ax
    add rdi, 2
    add edx, ecx
    dec esi
    jnz .one
.done:
    ret

; int doom_row_changed(const byte *row, byte *previous, int count)
; Whether a row of DOOM's picture differs from the copy of it kept from the last frame, which it
; then becomes: count bytes, a multiple of 32. Answers 1 when they differed, 0 when not.
doom_row_changed:
    vpxor ymm2, ymm2, ymm2          ; the bits that differ, of all the row
.thirty_two:
    vmovdqu ymm0, [rdi]
    vpxor ymm1, ymm0, [rsi]
    vpor ymm2, ymm2, ymm1
    vmovdqu [rsi], ymm0
    add rdi, 32
    add rsi, 32
    sub edx, 32
    jg .thirty_two
    xor eax, eax
    vptest ymm2, ymm2
    setnz al
    vzeroupper
    ret

; void doom_mix_add(int32_t *mix, const byte *samples, int count, int volume)
; One sound effect into the mix: mix[i] += (samples[i] - 128) * volume, eight samples at a time.
doom_mix_add:
    vmovd xmm5, ecx
    vpbroadcastd ymm5, xmm5
    vpbroadcastd ymm6, [silence]
.eight:
    cmp edx, 8
    jl .tail
    vpmovzxbd ymm0, qword [rsi]
    vpsubd ymm0, ymm0, ymm6
    vpmulld ymm0, ymm0, ymm5
    vpaddd ymm0, ymm0, [rdi]
    vmovdqu [rdi], ymm0
    add rsi, 8
    add rdi, 32
    sub edx, 8
    jmp .eight
.tail:
    vzeroupper
    test edx, edx
    jle .done
.one:
    movzx eax, byte [rsi]
    sub eax, 128
    imul eax, ecx
    add [rdi], eax
    inc rsi
    add rdi, 4
    dec edx
    jnz .one
.done:
    ret

; void doom_mix_out(int16_t *out, int32_t *mix, int count)
; The mix as 16-bit samples, each held to -32768..32767, sixteen at a time; the mix is left at
; zero for the next.
doom_mix_out:
    vpxor ymm7, ymm7, ymm7
.sixteen:
    cmp edx, 16
    jl .tail
    vmovdqu ymm0, [rsi]
    vpackssdw ymm0, ymm0, [rsi + 32]    ; quarters 0 2 1 3 of the sixteen, as AVX2 packs
    vpermq ymm0, ymm0, 0xd8             ; in order
    vmovdqu [rdi], ymm0
    vmovdqu [rsi], ymm7
    vmovdqu [rsi + 32], ymm7
    add rsi, 64
    add rdi, 32
    sub edx, 16
    jmp .sixteen
.tail:
    vzeroupper
    test edx, edx
    jle .done
.one:
    mov eax, [rsi]
    cmp eax, 32767
    jle .not_high
    mov eax, 32767
.not_high:
    cmp eax, -32768
    jge .not_low
    mov eax, -32768
.not_low:
    mov [rdi], ax
    mov dword [rsi], 0
    add rsi, 4
    add rdi, 2
    dec edx
    jnz .one
.done:
    ret

; void doom_palette_row(uint32_t *to, const byte *from, int count, const uint32_t *palette)
; A row of DOOM's picture into the window's pixels: to[i] = palette[from[i]].
doom_palette_row:
.thirty_two:                        ; four gathers a turn of the loop, which costs as much as one
    cmp edx, 32
    jl .eight
    vpmovzxbd ymm0, qword [rsi]
    vpcmpeqd ymm1, ymm1, ymm1
    vpgatherdd ymm2, [rcx + ymm0*4], ymm1
    vpmovzxbd ymm3, qword [rsi + 8]
    vpcmpeqd ymm1, ymm1, ymm1
    vpgatherdd ymm4, [rcx + ymm3*4], ymm1
    vpmovzxbd ymm5, qword [rsi + 16]
    vpcmpeqd ymm1, ymm1, ymm1
    vpgatherdd ymm6, [rcx + ymm5*4], ymm1
    vpmovzxbd ymm7, qword [rsi + 24]
    vpcmpeqd ymm1, ymm1, ymm1
    vpgatherdd ymm8, [rcx + ymm7*4], ymm1
    vmovdqu [rdi], ymm2
    vmovdqu [rdi + 32], ymm4
    vmovdqu [rdi + 64], ymm6
    vmovdqu [rdi + 96], ymm8
    add rsi, 32
    add rdi, 128
    sub edx, 32
    jmp .thirty_two
.eight:
    cmp edx, 8
    jl .tail
    vpmovzxbd ymm0, qword [rsi]
    vpcmpeqd ymm1, ymm1, ymm1
    vpgatherdd ymm2, [rcx + ymm0*4], ymm1
    vmovdqu [rdi], ymm2
    add rsi, 8
    add rdi, 32
    sub edx, 8
    jmp .eight
.tail:
    vzeroupper
    test edx, edx
    jle .done
.one:
    movzx eax, byte [rsi]
    mov eax, [rcx + rax*4]
    mov [rdi], eax
    inc rsi
    add rdi, 4
    dec edx
    jnz .one
.done:
    ret

; void doom_palette_row2(uint64_t *to, const byte *from, int count, const uint64_t *palette2)
; The same twice as wide: palette2 holds each colour twice, so to[i] = palette2[from[i]] is two
; pixels. A gather takes eight colours, the low halves of their palette2 entries, and two vpermd
; make sixteen pixels of them: seven instructions for eight of DOOM's pixels, where a vpgatherdq of
; four whole entries took eight.
doom_palette_row2:
    vmovdqu ymm8, [twin_low]
    vmovdqu ymm9, [twin_high]
.thirty_two:
    cmp edx, 32
    jl .eight
    vpmovzxbd ymm0, qword [rsi]
    vpcmpeqd ymm1, ymm1, ymm1
    vpgatherdd ymm2, [rcx + ymm0*8], ymm1
    vpermd ymm3, ymm8, ymm2
    vpermd ymm4, ymm9, ymm2
    vmovdqu [rdi], ymm3
    vmovdqu [rdi + 32], ymm4
    vpmovzxbd ymm0, qword [rsi + 8]
    vpcmpeqd ymm1, ymm1, ymm1
    vpgatherdd ymm2, [rcx + ymm0*8], ymm1
    vpermd ymm3, ymm8, ymm2
    vpermd ymm4, ymm9, ymm2
    vmovdqu [rdi + 64], ymm3
    vmovdqu [rdi + 96], ymm4
    vpmovzxbd ymm0, qword [rsi + 16]
    vpcmpeqd ymm1, ymm1, ymm1
    vpgatherdd ymm2, [rcx + ymm0*8], ymm1
    vpermd ymm3, ymm8, ymm2
    vpermd ymm4, ymm9, ymm2
    vmovdqu [rdi + 128], ymm3
    vmovdqu [rdi + 160], ymm4
    vpmovzxbd ymm0, qword [rsi + 24]
    vpcmpeqd ymm1, ymm1, ymm1
    vpgatherdd ymm2, [rcx + ymm0*8], ymm1
    vpermd ymm3, ymm8, ymm2
    vpermd ymm4, ymm9, ymm2
    vmovdqu [rdi + 192], ymm3
    vmovdqu [rdi + 224], ymm4
    add rsi, 32
    add rdi, 256
    sub edx, 32
    jmp .thirty_two
.eight:
    cmp edx, 8
    jl .tail
    vpmovzxbd ymm0, qword [rsi]
    vpcmpeqd ymm1, ymm1, ymm1
    vpgatherdd ymm2, [rcx + ymm0*8], ymm1
    vpermd ymm3, ymm8, ymm2
    vpermd ymm4, ymm9, ymm2
    vmovdqu [rdi], ymm3
    vmovdqu [rdi + 32], ymm4
    add rsi, 8
    add rdi, 64
    sub edx, 8
    jmp .eight
.tail:
    vzeroupper
    test edx, edx
    jle .done
.one:
    movzx eax, byte [rsi]
    mov rax, [rcx + rax*8]
    mov [rdi], rax
    inc rsi
    add rdi, 8
    dec edx
    jnz .one
.done:
    ret
