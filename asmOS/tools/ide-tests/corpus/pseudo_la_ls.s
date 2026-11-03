# la, and the loads and stores that name a label or an address
.data
w: .word 0x11223344
h: .half 0x8899
b: .byte 0x80
.align 2
dst: .space 16
.text
main:
    la   s0, w
    lw   s1, w
    lh   s2, h
    lhu  s3, h
    lb   s4, b
    lbu  s5, b
    lw   s6, (s0)
    lh   s7, (s0)
    lbu  s8, (s0)
    lw   s9, 268500992     # 0x10010000: lui and lw
    li   t0, 0x01020304
    sw   t0, dst, t1
