# Labels named like floating-point registers are refused, as RARS refuses them
.text
main:
    j     start
    li    s11, 1
start:
    jal   f1
    la    t0, f2
    jalr  t0
    la    t1, f3
    jr    t1
back3:
    la    t2, f4
    addi  t2, t2, -4
    jalr  t2, 4
    la    t3, f5
    addi  t3, t3, 8
    jalr  s6, -8(t3)
    call  f6
    li    a0, 1
    li    a7, 1
    ecall
    la    t4, f7
    jr    t4, 0
f1: li    s0, 1
    ret
f2: li    s1, 2
    ret
f3: li    s2, 3
    j     back3
f4: li    s3, 4
    ret
f5: li    s4, 5
    jr    s6
f6: li    s5, 6
    tail  f8
f7: li    a0, 7
    li    a7, 93
    ecall
f8: li    s7, 8
    ret
