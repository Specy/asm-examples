default rel
global _start

section .rodata
greeting:   db "Hello, world!", 10      ; 10 is the newline
GLEN        equ $ - greeting
answer:     db "The answer is 42", 10
ALEN        equ $ - answer

section .text
_start:
    mov rax, 1              ; syscall 1: write
    mov rdi, 1              ; to file descriptor 1, standard output
    lea rsi, [greeting]     ; the bytes to write
    mov rdx, GLEN           ; how many of them
    syscall

    mov rax, 1
    mov rdi, 1
    lea rsi, [answer]
    mov rdx, ALEN
    syscall

    mov rax, 60             ; syscall 60: exit
    xor rdi, rdi            ; with status 0
    syscall
