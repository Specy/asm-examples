default rel
global _start

section .data
sorted: dq 4, 8, 15, 16, 23, 42
COUNT   equ ($ - sorted) / 8

section .text
_start:
    mov rdx, 23             ; target
    xor rsi, rsi            ; low = 0
    mov rdi, COUNT          ; high = first position outside the array
    mov r8, -1              ; answer if absent
.search:
    cmp rsi, rdi
    jae .done               ; low >= high: empty range
    mov rcx, rdi
    sub rcx, rsi
    shr rcx, 1
    add rcx, rsi            ; mid = low + (high - low) / 2
    mov rax, [sorted + rcx*8]
    cmp rax, rdx
    je .found
    jl .higher              ; middle value is too small
    mov rdi, rcx            ; keep [low, mid)
    jmp .search
.higher:
    lea rsi, [rcx + 1]      ; keep [mid + 1, high)
    jmp .search
.found:
    mov r8, rcx
.done:
    mov rax, 60
    xor rdi, rdi
    syscall
