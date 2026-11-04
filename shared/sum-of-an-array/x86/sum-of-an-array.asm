default rel
global _start

section .data
numbers:    dq 4, 8, 15, 16, 23, 42
numbers_end:

section .text
_start:
    lea rsi, [numbers]      ; address of the first element
    lea rdi, [numbers_end]  ; address just after the last element
    xor rax, rax            ; running sum = 0
.next:
    add rax, [rsi]          ; add the qword at address rsi
    add rsi, 8              ; advance by one qword
    cmp rsi, rdi
    jb .next                ; repeat while rsi is below the end
    mov r8, rax             ; save the sum for inspection
    mov r9, rdi             ; save the end address for inspection

    mov rax, 60
    xor rdi, rdi
    syscall
