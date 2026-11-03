# sbrk hands out word-aligned blocks from 0x10040000
.text
main:
    li   a0, 10
    li   a7, 9
    ecall
    mv   s0, a0
    li   a0, 3
    ecall
    mv   s1, a0
    li   a0, 0
    ecall
    mv   s2, a0
    li   a0, 4096
    ecall
    mv   s3, a0
    li   t0, 0x5a5a
    sw   t0, 0(s3)
    sw   t0, 4092(s3)
    lw   s4, 4092(s3)
    mv   a0, s1
    li   a7, 34
    ecall
