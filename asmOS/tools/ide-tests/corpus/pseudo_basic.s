# The arithmetic pseudo-instructions
.text
main:
    li     t0, -5
    li     t1, 7
    nop
    not    s0, t0
    mv     s1, t1
    neg    s2, t0
    sgt    s3, t1, t0
    sgtu   s4, t1, t0
    seqz   s5, zero
    seqz   s6, t1
    snez   s7, t1
    sgtz   s8, t0
    sltz   s9, t0
    zext.b s10, t0
    neg    s11, zero
    mv     a0, s0
    li     a7, 1
    ecall
