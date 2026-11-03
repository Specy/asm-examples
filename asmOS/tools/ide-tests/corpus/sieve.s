# The sieve of Eratosthenes below 2000, on the heap: how many primes, and the last ten
.text
main:
    li   s0, 2000
    mv   a0, s0
    li   a7, 9
    ecall
    mv   s1, a0             # flags, zero from the start
    li   t0, 2
outer:
    mul  t1, t0, t0
    bge  t1, s0, counted
    add  t2, s1, t0
    lbu  t3, 0(t2)
    bnez t3, next
mark:
    bge  t1, s0, next
    add  t2, s1, t1
    li   t4, 1
    sb   t4, 0(t2)
    add  t1, t1, t0
    j    mark
next:
    addi t0, t0, 1
    j    outer
counted:
    li   s2, 0
    li   t0, 2
count:
    bge  t0, s0, show
    add  t2, s1, t0
    lbu  t3, 0(t2)
    bnez t3, skip
    addi s2, s2, 1
    mv   s3, t0
skip:
    addi t0, t0, 1
    j    count
show:
    mv   a0, s2
    li   a7, 1
    ecall
    li   a0, ' '
    li   a7, 11
    ecall
    mv   a0, s3
    li   a7, 1
    ecall
