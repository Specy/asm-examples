# Euclid's algorithm with rem, on a few pairs
.data
pairs: .word 48, 18, 1071, 462, 17, 5, 0, 9, 270, 192
.text
main:
    la   s0, pairs
    li   s1, 5
loop:
    lw   a0, 0(s0)
    lw   a1, 4(s0)
    jal  gcd
    li   a7, 1
    ecall
    li   a0, ';'
    li   a7, 11
    ecall
    addi s0, s0, 8
    addi s1, s1, -1
    bnez s1, loop
    li   a7, 10
    ecall
gcd:
    beqz a1, gcd_done
    rem  t0, a0, a1
    mv   a0, a1
    mv   a1, t0
    j    gcd
gcd_done:
    ret
