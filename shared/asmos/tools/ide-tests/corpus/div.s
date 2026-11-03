# Division and remainder: by zero, INT_MIN / -1, and the signs
.text
main:
    li   t0, -7
    li   t1, 2
    li   t2, 0x80000000
    li   t3, -1
    div  s0, t0, t1        # -3: rounds toward zero
    rem  s1, t0, t1        # -1
    divu s2, t0, t1
    remu s3, t0, t1
    div  s4, t0, zero      # -1
    divu s5, t0, zero      # all ones
    rem  s6, t0, zero      # the dividend
    remu s7, t0, zero
    div  s8, t2, t3        # INT_MIN
    rem  s9, t2, t3        # 0
    divu s10, t2, t3
    remu s11, t2, t3
    li   t4, 7
    li   t5, -2
    div  a1, t4, t5
    rem  a2, t4, t5
    div  a3, zero, t5
    mv   a0, s0
    li   a7, 1
    ecall
