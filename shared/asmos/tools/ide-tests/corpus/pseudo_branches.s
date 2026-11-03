# The branch pseudo-instructions, taken and not
.text
main:
    li   s0, 0
    li   t0, -3
    li   t1, 4
    b    l1
    addi s0, s0, 1000
l1: beqz zero, l2
    addi s0, s0, 1000
l2: bnez t0, l3
    addi s0, s0, 1000
l3: bgez t1, l4
    addi s0, s0, 1000
l4: bltz t0, l5
    addi s0, s0, 1000
l5: bgtz t1, l6
    addi s0, s0, 1000
l6: blez t0, l7
    addi s0, s0, 1000
l7: bgt  t1, t0, l8
    addi s0, s0, 1000
l8: bgtu t0, t1, l9       # 0xfffffffd > 4 unsigned
    addi s0, s0, 1000
l9: ble  t0, t1, l10
    addi s0, s0, 1000
l10: bleu t1, t0, l11
    addi s0, s0, 1000
l11: beqz t0, l12
    addi s0, s0, 1
l12: bgtz t0, l13
    addi s0, s0, 2
l13: bgt t0, t1, l14
    addi s0, s0, 4
l14: bleu t0, t1, l15
    addi s0, s0, 8
l15: mv a0, s0
    li   a7, 1
    ecall
