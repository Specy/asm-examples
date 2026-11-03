# I-type ALU instructions with the immediates at the ends of their ranges
.text
main:
    li    t0, 100
    addi  s0, t0, -2048
    addi  s1, t0, 2047
    slti  s2, t0, 101
    slti  s3, t0, -1
    li    t1, -5
    sltiu s4, t1, -1       # -1 as unsigned is the largest
    sltiu s5, t0, 5
    xori  s6, t0, -1
    ori   s7, t0, 0x7ff
    andi  s8, t1, 0x0f0
    slli  s9, t1, 31
    srli  s10, t1, 28
    srai  s11, t1, 1
    srai  a1, t1, 31
    slli  a2, t0, 0
    addi  a3, zero, -1
    andi  a4, t1, -2048
    addi  zero, t0, 5      # x0 stays 0
    mv    a0, s0
    li    a7, 1
    ecall
