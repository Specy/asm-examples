# read character: one at a time, Enter as 10
.text
main:
    li   s1, 6
    li   s0, 0
loop:
    li   a7, 12
    ecall
    slli s0, s0, 4
    add  s0, s0, a0
    li   a7, 1
    ecall
    li   a0, ','
    li   a7, 11
    ecall
    addi s1, s1, -1
    bnez s1, loop
