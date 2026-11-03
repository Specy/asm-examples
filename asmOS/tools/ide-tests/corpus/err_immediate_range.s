# Immediates out of range
.text
main:
    addi t0, t0, 5000
    slli t1, t1, 32
    addi t2, t2, 2048
    lui  t3, 0x100000
    sltiu t4, t4, -2049
    andi t5, t5, 0xfff
    lw   t6, 3000(sp)
    srai s0, s0, -1
