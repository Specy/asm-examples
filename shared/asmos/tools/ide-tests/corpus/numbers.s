# Number spellings: hexadecimal, octal, signs, and wrapping hexadecimal
.text
main:
    li   s0, 0xFF
    li   s1, 0Xff
    li   s2, 010
    li   s3, -0x10
    li   s4, +0x10
    li   s5, 0
    li   s6, 0x80000000
    li   s7, -2147483648
    li   s8, 0xdeadbeef
    addi s9, zero, -0x800
    andi s10, s8, 0x7ff
    li   s11, 0777
