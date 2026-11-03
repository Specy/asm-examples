# R-type ALU instructions on edge values; the registers at the end are compared
.text
main:
    li   t0, 0x7fffffff
    li   t1, 1
    li   t2, -1
    li   t3, 0x80000000
    add  s0, t0, t1        # overflow wraps to INT_MIN
    sub  s1, t3, t1        # wraps to INT_MAX
    sub  s2, zero, t3      # -INT_MIN is INT_MIN
    slt  s3, t3, t0        # signed: INT_MIN < INT_MAX
    sltu s4, t3, t0        # unsigned: 0x80000000 > 0x7fffffff
    slt  s5, t2, t1
    sltu s6, t2, t1
    xor  s7, t0, t2
    or   s8, t3, t1
    and  s9, t2, t0
    sll  s10, t1, t0       # shift by 31 (low five bits)
    srl  s11, t2, t1
    sra  a1, t3, t1
    sra  a2, t2, t0
    srl  a3, t3, t0
    add  a4, t2, t2
    sub  a5, t1, t2
    mv   a0, s0
    li   a7, 1
    ecall
    li   a0, 10
    li   a7, 11
    ecall
