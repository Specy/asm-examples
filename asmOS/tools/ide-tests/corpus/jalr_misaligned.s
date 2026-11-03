# jalr clears bit 0 of its target; bit 1 makes an address no instruction is at
.text
main:
    la   t0, there
    addi t0, t0, 1
    jalr t1, t0, 0
    li   s1, 9
there:
    li   s0, 1
    addi t0, t0, 1
    jr   t0
