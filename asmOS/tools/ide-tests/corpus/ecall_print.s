# The print services on the values where formats differ
.data
nl: .asciz "\n"
.text
main:
    li   s0, -2147483648
    li   s1, -1
    li   s2, 0
    li   s3, 255
    li   s4, 12345
    li   a7, 1
    mv   a0, s0
    ecall
    jal  newline
    mv   a0, s1
    ecall
    jal  newline
    li   a7, 34
    mv   a0, s1
    ecall
    jal  newline
    mv   a0, s3
    ecall
    jal  newline
    li   a7, 35
    mv   a0, s4
    ecall
    jal  newline
    mv   a0, s0
    ecall
    jal  newline
    li   a7, 36
    mv   a0, s1
    ecall
    jal  newline
    mv   a0, s2
    ecall
    jal  newline
    li   a7, 11
    li   a0, 'Z'
    ecall
    li   a0, 0x141         # only the low byte: 'A'
    ecall
    li   a0, 233           # a byte past ASCII prints as its Latin-1 character
    ecall
    jal  newline
    li   a7, 10
    ecall
newline:
    mv   t6, a7
    la   a0, nl
    li   a7, 4
    ecall
    mv   a7, t6
    ret
