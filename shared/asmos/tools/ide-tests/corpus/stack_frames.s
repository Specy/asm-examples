# Stack frames with negative offsets, and the stack's low end
.text
main:
    addi sp, sp, -16
    li   t0, 11
    sw   t0, 12(sp)
    sw   ra, 8(sp)
    lw   s0, 12(sp)
    li   t1, 0x7fc00000    # the lowest stack address RARS keeps
    sw   t0, 0(t1)
    lw   s1, 0(t1)
    lw   ra, 8(sp)
    addi sp, sp, 16
    li   t2, 0x7fbffffc    # just below it
    lw   s2, 0(t2)
