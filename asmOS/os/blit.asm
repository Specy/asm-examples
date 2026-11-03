; The desktop's pixel loops, written by hand. Everything the desktop draws ends in one of these: a
; rectangle filled with one colour, or a rectangle copied from one picture to another. Each row is
; a single `rep` instruction, which the emulator runs as one, many times faster than the same loop in
; C; and `rep movsb` the fastest of them by far (asmOS DOOM's Game > Benchmark CPU: 3440 MB/s, where
; `rep movsd` copied 202 and `rep stosd` filled 336), so rows are copied by bytes, and a fill
; copies its first row into the rest. The C code declares them in os.h.
;
; Arguments follow the System V calling convention: rdi, rsi, rdx, rcx, r8, r9. A stride is the
; distance in bytes from the start of one row to the start of the next.

    default rel
    section .text

    global fill_rect32
    global copy_rect32
    global move_rect32

; void fill_rect32(uint32_t *to, size_t stride, size_t width, size_t height, uint32_t colour)
fill_rect32:
    test rdx, rdx
    jz .done
    test rcx, rcx
    jz .done
    mov r9, rcx                 ; rows left
    mov eax, r8d                ; the colour rep stosd stores
    mov r8, rsi                 ; the stride, since rep movsb needs rsi
    mov r10, rdi                ; the first row, which the others copy
    mov rcx, rdx
    rep stosd
    shl rdx, 2                  ; the row's bytes
    mov r11, r10                ; the row being filled
.row:
    dec r9
    jz .done
    add r11, r8
    mov rdi, r11
    mov rsi, r10
    mov rcx, rdx
    rep movsb
    jmp .row
.done:
    ret

; void copy_rect32(uint32_t *to, size_t to_stride, const uint32_t *from, size_t from_stride,
;                  size_t width, size_t height)
; A from_stride of 0 copies the same row into every row of the destination.
copy_rect32:
    test r8, r8
    jz .done
    test r9, r9
    jz .done
    mov rax, rsi                ; rep movsb needs rsi, so the strides move to rax and rdx
    mov r10, rdi
    mov r11, rdx
    mov rdx, rcx
    shl r8, 2                   ; the row's bytes, for rep movsb
.row:
    mov rdi, r10
    mov rsi, r11
    mov rcx, r8
    rep movsb
    add r10, rax
    add r11, rdx
    dec r9
    jnz .row
.done:
    ret

; void move_rect32(uint32_t *to, const uint32_t *from, size_t stride, size_t width, size_t height)
; Copies a rectangle within one picture when the source and the destination may overlap, as a
; window dragged a few pixels does: rows go bottom-up when moving down, and each row backwards when
; moving right, so no pixel is overwritten before it is read.
move_rect32:
    test rcx, rcx
    jz .done
    test r8, r8
    jz .done
    mov r9, rcx                 ; the width, since rep needs rcx
    mov r10, rdi
    mov r11, rsi
    cmp rdi, rsi
    jbe .forward
    ; moving down or right: start from the last row, and copy each row from its last pixel
    lea rax, [r8 - 1]
    imul rax, rdx
    add r10, rax
    add r11, rax
    lea rax, [r9*4 - 4]
    std
.back_row:
    lea rdi, [r10 + rax]
    lea rsi, [r11 + rax]
    mov rcx, r9
    rep movsd
    sub r10, rdx
    sub r11, rdx
    dec r8
    jnz .back_row
    cld                         ; the calling convention wants the direction flag clear
    ret
.forward:
    shl r9, 2                   ; the row's bytes, for rep movsb
.forward_row:
    mov rdi, r10
    mov rsi, r11
    mov rcx, r9
    rep movsb
    add r10, rdx
    add r11, rdx
    dec r8
    jnz .forward_row
.done:
    ret
