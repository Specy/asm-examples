# A 3 by 3 matrix product, row by row
.data
A: .word 1, 2, 3, 4, 5, 6, 7, 8, 9
B: .word 9, 8, 7, 6, 5, 4, 3, 2, -1
C: .space 36
.text
main:
    la   s0, A
    la   s1, B
    la   s2, C
    li   t0, 0              # row
rows:
    li   t1, 0              # column
cols:
    li   t2, 0              # k
    li   t3, 0              # sum
dot:
    li   t4, 3
    mul  t5, t0, t4
    add  t5, t5, t2
    slli t5, t5, 2
    add  t5, s0, t5
    lw   t5, 0(t5)
    mul  t6, t2, t4
    add  t6, t6, t1
    slli t6, t6, 2
    add  t6, s1, t6
    lw   t6, 0(t6)
    mul  t5, t5, t6
    add  t3, t3, t5
    addi t2, t2, 1
    blt  t2, t4, dot
    mul  t5, t0, t4
    add  t5, t5, t1
    slli t5, t5, 2
    add  t5, s2, t5
    sw   t3, 0(t5)
    mv   a0, t3
    li   a7, 1
    ecall
    li   a0, ' '
    li   a7, 11
    ecall
    addi t1, t1, 1
    li   t4, 3
    blt  t1, t4, cols
    addi t0, t0, 1
    blt  t0, t4, rows
