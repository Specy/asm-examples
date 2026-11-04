default rel
global _start

section .text
; sum7(a, b, c, d, e, f, g) -> the total
sum7:
    push rbp                ; save the caller's frame pointer
    mov rbp, rsp            ; fixed base for this frame
    sub rsp, 16             ; reserve two local qwords
    push rbx                ; save the caller's rbx before using it

    mov rbx, rdi            ; total of the first six arguments
    add rbx, rsi
    add rbx, rdx
    add rbx, rcx
    add rbx, r8
    add rbx, r9
    mov [rbp - 8], rbx      ; write that total to the first local
    add rbx, [rbp + 16]     ; add the seventh argument
    mov rax, rbx            ; return the answer

    pop rbx                 ; restore the caller's rbx
    leave                   ; mov rsp, rbp, then pop rbp
    ret

_start:
    mov rdi, 1
    mov rsi, 2
    mov rdx, 3
    mov rcx, 4
    mov r8, 5
    mov r9, 6
    sub rsp, 8              ; padding: _start began with rsp aligned to 16
    push 7                  ; seventh argument; rsp is aligned again
    call sum7
    add rsp, 16             ; remove the argument and padding
    mov r12, rax            ; keep the answer, 28, visible in the register view

    mov rax, 60
    xor rdi, rdi
    syscall
