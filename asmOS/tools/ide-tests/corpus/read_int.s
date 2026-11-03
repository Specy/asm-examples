# read integer: lines trimmed, signs, the largest and smallest ints
.text
main:
    li   s0, 0
    li   s1, 5
loop:
    li   a7, 5
    ecall
    add  s0, s0, a0
    li   a7, 1
    ecall
    li   a0, ' '
    li   a7, 11
    ecall
    addi s1, s1, -1
    bnez s1, loop
    mv   a0, s0
    li   a7, 1
    ecall
