# The jump pseudo-instructions: j, jal, jr, jalr, ret, call and tail
.text
main:
    j     start
    li    s11, 1
start:
    jal   fn1
    la    t0, fn2
    jalr  t0
    la    t1, fn3
    jr    t1
back3:
    la    t2, fn4
    addi  t2, t2, -4
    jalr  t2, 4
    la    t3, fn5
    addi  t3, t3, 8
    jalr  s6, -8(t3)
    call  fn6
    li    a0, 1
    li    a7, 1
    ecall
    la    t4, fn7
    jr    t4, 0
fn1: li    s0, 1
    ret
fn2: li    s1, 2
    ret
fn3: li    s2, 3
    j     back3
fn4: li    s3, 4
    ret
fn5: li    s4, 5
    jr    s6
fn6: li    s5, 6
    tail  fn8
fn7: li    a0, 7
    li    a7, 93
    ecall
fn8: li    s7, 8
    ret
