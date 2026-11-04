default rel
global _start

section .bss
fact_out:   resq 1
fib_out:    resq 1

section .text
; fact(n) -> n!, for n >= 0
fact:
    cmp rdi, 1
    jg .recurse
    mov rax, 1              ; 0! and 1! are 1
    ret
.recurse:
    push rdi                ; save this call's n; rsp is now aligned
    dec rdi
    call fact               ; rax = fact(n - 1)
    pop rdi                 ; recover this call's n
    imul rax, rdi           ; n * fact(n - 1)
    ret

; fib(n) -> the nth Fibonacci number, for n >= 0
fib:
    cmp rdi, 2
    jge .recurse
    mov rax, rdi            ; fib(0) = 0 and fib(1) = 1
    ret
.recurse:
    push rbx                ; preserve the caller's rbx; rsp is now aligned
    sub rsp, 16             ; local space; rsp stays aligned
    mov [rsp], rdi          ; save this call's n in its local space

    dec rdi
    call fib                ; rax = fib(n - 1)
    mov rbx, rax            ; keep the first answer across the second call

    mov rdi, [rsp]          ; reload this call's n
    sub rdi, 2
    call fib                ; rax = fib(n - 2)
    add rax, rbx            ; combine the two answers

    add rsp, 16             ; release local space
    pop rbx                 ; return the caller's rbx unchanged
    ret

_start:
    mov rdi, 10
    call fact
    mov [fact_out], rax

    mov rdi, 10
    call fib
    mov [fib_out], rax

    mov rax, 60
    xor rdi, rdi
    syscall
