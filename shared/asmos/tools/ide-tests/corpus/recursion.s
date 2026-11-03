# Recursive Fibonacci and factorial with stack frames
.text
main:
    li   s0, 0
loop:
    mv   a0, s0
    call fib
    li   a7, 1
    ecall
    li   a0, ' '
    li   a7, 11
    ecall
    addi s0, s0, 1
    li   t0, 15
    blt  s0, t0, loop
    li   a0, 12
    call fact
    li   a7, 1
    ecall
    li   a7, 10
    ecall
fib:                        # fib(n) = fib(n-1) + fib(n-2)
    li   t0, 2
    blt  a0, t0, fib_small
    addi sp, sp, -12
    sw   ra, 8(sp)
    sw   s1, 4(sp)
    sw   a0, 0(sp)
    addi a0, a0, -1
    call fib
    mv   s1, a0
    lw   a0, 0(sp)
    addi a0, a0, -2
    call fib
    add  a0, a0, s1
    lw   s1, 4(sp)
    lw   ra, 8(sp)
    addi sp, sp, 12
fib_small:
    ret
fact:
    li   t0, 1
    ble  a0, t0, fact_one
    addi sp, sp, -8
    sw   ra, 4(sp)
    sw   a0, 0(sp)
    addi a0, a0, -1
    jal  fact
    lw   t1, 0(sp)
    mul  a0, a0, t1
    lw   ra, 4(sp)
    addi sp, sp, 8
    ret
fact_one:
    li   a0, 1
    ret
