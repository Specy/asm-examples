# read string with fgets's limits: buffer sizes 1, 2, 4 and 40
.data
buf: .space 64
.text
main:
    la   s0, buf
    li   s1, 0xaa
    li   t0, 0
fill:
    add  t1, s0, t0
    sb   s1, 0(t1)
    addi t0, t0, 1
    li   t2, 64
    blt  t0, t2, fill
    mv   a0, s0
    li   a1, 1
    li   a7, 8
    ecall
    addi a0, s0, 4
    li   a1, 2
    ecall
    addi a0, s0, 8
    li   a1, 4
    ecall
    addi a0, s0, 16
    li   a1, 40
    ecall
    addi a0, s0, 16
    li   a7, 4
    ecall
    addi a0, s0, 8
    ecall
    lw   s2, 0(s0)
    lw   s3, 4(s0)
    lw   s4, 8(s0)
    lw   s5, 12(s0)
