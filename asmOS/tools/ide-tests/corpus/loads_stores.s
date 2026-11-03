# Loads and stores of every width: sign and zero extension, byte order
.data
bytes: .byte 0x80, 0x7f, 0xff, 0x01
halves: .half 0x8001, 0x7fff
word: .word 0xdeadbeef
buffer: .space 16
.text
main:
    la   t0, bytes
    lb   s0, 0(t0)
    lbu  s1, 0(t0)
    lb   s2, 1(t0)
    lb   s3, 2(t0)
    lbu  s4, 3(t0)
    la   t1, halves
    lh   s5, 0(t1)
    lhu  s6, 0(t1)
    lh   s7, 2(t1)
    la   t2, word
    lw   s8, 0(t2)
    lbu  s9, 0(t2)         # little endian: 0xef
    lhu  s10, 2(t2)
    la   t3, buffer
    sw   s8, 0(t3)
    sh   s8, 4(t3)
    sb   s8, 6(t3)
    li   t4, -2
    sh   t4, 8(t3)
    sb   t4, 11(t3)
    lw   s11, 4(t3)
    lw   a1, 8(t3)
    lh   a2, 8(t3)
    lb   a3, 11(t3)
    sw   t4, -4(t3)        # a negative offset
    lw   a4, -4(t3)
    mv   a0, s11
    li   a7, 34
    ecall
