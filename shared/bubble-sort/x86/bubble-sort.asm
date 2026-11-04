default rel
global _start

section .data
values: dq 42, 4, 23, 8, 16, 15
COUNT   equ ($ - values) / 8

section .text
_start:
    mov rcx, COUNT
    dec rcx                 ; six elements need at most five passes
.outer:
    xor rbx, rbx            ; start this pass at index 0
.inner:
    mov rax, [values + rbx*8]       ; left qword
    mov rdx, [values + rbx*8 + 8]   ; right qword
    cmp rax, rdx
    jle .ordered                    ; signed: left <= right
    mov [values + rbx*8], rdx       ; put the smaller value on the left
    mov [values + rbx*8 + 8], rax
.ordered:
    inc rbx
    cmp rbx, rcx
    jb .inner
    dec rcx                 ; next pass has one fewer pair to check
    jnz .outer

    mov rax, 60
    xor rdi, rdi
    syscall
