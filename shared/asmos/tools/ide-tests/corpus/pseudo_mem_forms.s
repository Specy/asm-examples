# Every load and store form: through a register, an absolute address, a 32-bit address
.data
w: .word 0x80818283, 0x01020304
.text
main:
    la   t0, w
    lw   s0, (t0)
    lh   s1, (t0)
    lhu  s2, (t0)
    lb   s3, (t0)
    lbu  s4, (t0)
    lw   s5, 268500996       # w + 4
    lh   s6, 268500994
    lhu  s7, 268500994
    lb   s8, 268500993
    lbu  s9, 268500993
    li   t1, 0x55667788
    sw   t1, (t0)
    lw   s10, (t0)
    sw   t1, 268500996, t2
    lw   s11, 268500996
    sh   t1, 268500992, t3
    sb   t1, 268500993, t3
    lw   a1, (t0)
    lw   a2, -96            # the memory map's top: still zero
    sw   t1, -96
    lw   a3, -96
