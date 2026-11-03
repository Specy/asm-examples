# The store pseudo-instructions: a label with a temporary, a register, an address
.data
dst: .space 32
.text
main:
    li   t0, 0x01020304
    sw   t0, dst, t1
    la   t2, dst
    sh   t0, (t2)
    sb   t0, (t2)
    li   t3, 0xa0b0c0d0
    sw   t3, 268500996, t4  # dst + 4 by its address
    sh   t3, 268501000, t4
    sb   t3, 268501004, t4
    sh   t3, dst, t5
    sb   t3, dst, t5
    lw   s0, 0(t2)
    lw   s1, 4(t2)
    lw   s2, 8(t2)
    lw   s3, 12(t2)
    sw   t0, -100          # 0xffffff9c, in the memory map
    lw   s4, -100
    lh   s5, -100
    lhu  s6, -98
    lb   s7, -97
    lbu  s8, -97
    sh   t0, -96
    sb   t0, -94
    lw   s9, -96
