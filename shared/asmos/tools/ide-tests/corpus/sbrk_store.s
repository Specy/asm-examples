# Heap memory reads as zero until written, and is plain memory
.text
main:
    li   a0, 64
    li   a7, 9
    ecall
    mv   s0, a0
    lw   s1, 60(s0)
    li   t0, -1
    sw   t0, 60(s0)
    lw   s2, 60(s0)
    lb   s3, 63(s0)
    li   a0, 1
    li   a7, 9
    ecall
    sub  s4, a0, s0
