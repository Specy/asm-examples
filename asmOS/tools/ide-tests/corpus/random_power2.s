# nextInt(bound) for powers of two takes the high bits
.text
main:
    li   a0, 1
    li   a1, 99
    li   a7, 40
    ecall
    li   s0, 0
loop:
    li   a0, 1
    sll  a1, s0, s0
    addi a1, a1, 1
    li   a1, 16
    li   a7, 42
    ecall
    li   a7, 1
    ecall
    li   a0, 1
    li   a1, 0x40000000
    li   a7, 42
    ecall
    li   a7, 1
    ecall
    li   a0, 1
    li   a1, 0x7fffffff
    li   a7, 42
    ecall
    li   a7, 1
    ecall
    li   a0, ','
    li   a7, 11
    ecall
    addi s0, s0, 1
    li   t0, 4
    blt  s0, t0, loop
