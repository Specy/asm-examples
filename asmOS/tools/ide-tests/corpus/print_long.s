# A long output: the numbers 0 to 1999
.text
main:
    li   s0, 0
loop:
    mv   a0, s0
    li   a7, 1
    ecall
    li   a0, ' '
    li   a7, 11
    ecall
    addi s0, s0, 1
    li   t0, 2000
    blt  s0, t0, loop
