default rel
global _start

section .data
text:   db "assembly", 0
LEN     equ $ - text - 1        ; eight, without the terminator

section .text
_start:
    lea rsi, [text]             ; left, at the first character
    lea rdi, [text + LEN - 1]   ; right, at the last
.swap:
    cmp rsi, rdi
    jae .done                   ; they have met or crossed
    mov al, [rsi]
    mov bl, [rdi]
    mov [rsi], bl               ; the two bytes, exchanged
    mov [rdi], al
    inc rsi
    dec rdi
    jmp .swap
.done:

    mov rax, 60
    xor rdi, rdi
    syscall
