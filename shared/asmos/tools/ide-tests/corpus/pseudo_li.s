# li in both of its forms, on the values where the halves round
.text
main:
    li s0, 0
    li s1, -2048
    li s2, 2047
    li s3, 2048            # past 12 signed bits: lui and addi
    li s4, -2049
    li s5, 0x7fffffff
    li s6, 0x80000000
    li s7, 0xffffffff      # -1: one addi
    li s8, 0x12345fff      # bit 11 set: the upper half rounds up
    li s9, 0xfffff800
    li s10, 4095
    li s11, 0x10010000
    li a1, -2147483648
    li a2, 01777           # octal
    li a3, 0x800
    li a4, 9999999999      # RARS reads this as 1410065407
    li a5, +12
    mv a0, s8
    li a7, 34
    ecall
