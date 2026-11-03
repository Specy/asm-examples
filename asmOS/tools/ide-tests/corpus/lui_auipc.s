# lui and auipc, with the 20-bit operand's spellings
.text
main:
    lui   s0, 0x12345
    lui   s1, 0xfffff
    lui   s2, 0
    lui   s3, 1
    lui   s4, -1          # a 12-bit value is allowed where 20 bits are
    auipc s5, 0
    auipc s6, 1
    auipc s7, 0xfffff
    lui   s8, 1048575
    mv    a0, s0
    li    a7, 34
    ecall
