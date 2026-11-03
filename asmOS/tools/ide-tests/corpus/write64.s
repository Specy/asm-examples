# write (64) to standard output and error
.data
text: .ascii "abc\0def"
.text
main:
    li   a0, 1
    la   a1, text
    li   a2, 3
    li   a7, 64
    ecall
    mv   s0, a0
    li   a0, 2
    la   a1, text
    addi a1, a1, 4
    li   a2, 3
    ecall
    mv   s1, a0
    li   a0, 1
    la   a1, text
    li   a2, -1
    ecall
    mv   s2, a0
