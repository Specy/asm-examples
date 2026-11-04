default rel
global _start

section .rodata
digits: db "0123456789abcdefghijklmnopqrstuvwxyz"
nl:     db 10

section .bss
buffer: resb 64

section .text
; print_number(unsigned value in rdi, base 2..36 in rsi)
print_number:
    lea rcx, [buffer + 64]  ; one past the end of the buffer
    lea r8, [digits]
    mov rax, rdi
    test rax, rax
    jnz .divide
    dec rcx                 ; the division loop produces no digits for zero
    mov byte [rcx], '0'
    jmp .print
.divide:
    xor rdx, rdx            ; the high half of the dividend
    div rsi                 ; rax = value / base, rdx = value % base
    mov r9b, [r8 + rdx]     ; remainder 11 selects digits[11], 'b'
    dec rcx
    mov [rcx], r9b          ; written backwards, from the end of the buffer
    test rax, rax
    jnz .divide             ; until there is nothing left to divide
.print:
    lea rdx, [buffer + 64]
    sub rdx, rcx            ; how many characters were written
    mov rsi, rcx            ; where they start
    mov rax, 1              ; write
    mov rdi, 1
    syscall

    mov rax, 1              ; and a newline after them
    mov rdi, 1
    lea rsi, [nl]
    mov rdx, 1
    syscall
    ret

_start:
    mov rdi, 12345
    mov rsi, 10             ; in decimal
    call print_number

    mov rdi, 12345
    mov rsi, 16             ; in hex
    call print_number

    mov rdi, 12345
    mov rsi, 2              ; and in binary
    call print_number

    mov rax, 60
    xor rdi, rdi
    syscall
