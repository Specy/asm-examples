.eqv COUNT, 8

.data
numbers: .word 42, 8, 15, 4, 23, 16, 99, 1

.text
main:
    li t0, COUNT
    addi t0, t0, -1         # outer passes remaining: COUNT - 1
outer:
    la t1, numbers          # start this pass at the first pair
    mv t2, t0               # pair comparisons in this pass
inner:
    lw t3, 0(t1)            # left value
    lw t4, 4(t1)            # right value
    bge t4, t3, in_order    # signed right >= left: no swap
    sw t4, 0(t1)
    sw t3, 4(t1)
in_order:
    addi t1, t1, 4          # move to the next overlapping pair
    addi t2, t2, -1
    bnez t2, inner
    addi t0, t0, -1
    bnez t0, outer
