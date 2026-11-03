# The M extension's multiplications, every sign combination
.text
main:
    li     t0, -7
    li     t1, 3
    li     t2, 0x80000000
    li     t3, 0xffffffff
    li     t4, 0x12345678
    mul    s0, t0, t1
    mulh   s1, t0, t1
    mulhsu s2, t0, t1
    mulhu  s3, t0, t1
    mul    s4, t2, t2
    mulh   s5, t2, t2
    mulhu  s6, t3, t3
    mulhsu s7, t3, t3      # -1 times 0xffffffff
    mulhsu s8, t1, t3
    mulh   s9, t4, t4
    mulhu  s10, t4, t2
    mul    s11, t4, t3
    mulhsu a1, t2, t2
    mv     a0, s1
    li     a7, 1
    ecall
