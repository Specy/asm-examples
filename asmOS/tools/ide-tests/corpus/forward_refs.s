# Labels used before they are defined: in data, in branches, and a jump table of text labels
.data
table: .word case0, case1, case2
ptr:   .word later
later: .word 0x55
.text
main:
    li   s0, 0
    li   s1, 0
next:
    la   t0, table
    slli t1, s1, 2
    add  t0, t0, t1
    lw   t2, 0(t0)
    jr   t2
case0:
    addi s0, s0, 1
    j    joined
case1:
    addi s0, s0, 10
    j    joined
case2:
    addi s0, s0, 100
joined:
    addi s1, s1, 1
    li   t3, 3
    blt  s1, t3, next
    lw   t4, ptr
    lw   s2, 0(t4)
    beqz zero, forward
    li   s0, -1
forward:
    mv   a0, s0
    li   a7, 1
    ecall
