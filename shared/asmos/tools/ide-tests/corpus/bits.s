# Bit tricks: count the ones, swap with xor, reverse the bits
.text
main:
    li   s0, 0xf0f01234
    mv   t0, s0
    li   s1, 0
ones:
    beqz t0, counted
    andi t1, t0, 1
    add  s1, s1, t1
    srli t0, t0, 1
    j    ones
counted:
    li   s2, 5
    li   s3, 9
    xor  s2, s2, s3
    xor  s3, s2, s3
    xor  s2, s2, s3
    mv   t0, s0
    li   s4, 0
    li   t2, 32
reverse:
    slli s4, s4, 1
    andi t1, t0, 1
    or   s4, s4, t1
    srli t0, t0, 1
    addi t2, t2, -1
    bnez t2, reverse
    mv   a0, s4
    li   a7, 35
    ecall
