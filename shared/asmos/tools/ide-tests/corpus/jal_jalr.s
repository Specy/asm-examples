# jal and jalr in their basic forms, links included
.text
main:
    jal   ra, first
    li    s1, 11
    jal   t0, second
    li    s2, 22
    la    t1, third
    jalr  t2, t1, 0
    li    s3, 33
    la    t1, fourth
    jalr  t3, t1, 8        # skips two instructions of fourth
    li    s4, 44
    j     done
first:
    li    s0, 1
    jalr  zero, ra, 0
second:
    jalr  zero, t0, 0
third:
    jalr  zero, t2, 0
fourth:
    li    s5, 1
    li    s5, 2
    li    s6, 3
    jalr  x0, t3, 0
done:
    mv    a0, s0
    li    a7, 1
    ecall
