# Register shifts use only the low five bits of the amount
.text
main:
    li  t0, 0x80000001
    li  t1, 32
    li  t2, 33
    li  t3, -1
    li  t4, 63
    sll s0, t0, t1
    sll s1, t0, t2
    srl s2, t0, t2
    sra s3, t0, t2
    sll s4, t0, t3
    srl s5, t0, t3
    sra s6, t0, t3
    sra s7, t0, t4
    srl s8, t3, t4
    sll s9, t3, zero
    li  a7, 34
    mv  a0, s3
    ecall
