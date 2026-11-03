# %hi and %lo: addresses built the way a C compiler builds them
.data
pad: .space 2044
x:   .word 0x0badf00d
y:   .word 0
.text
main:
    lui  t0, %hi(x)
    addi t0, t0, %lo(x)
    lw   s0, 0(t0)
    lui  t1, %hi(x)
    lw   s1, %lo(x)(t1)
    lui  t2, %hi(y)
    sw   s0, %lo(y)(t2)
    lw   s2, %lo(y)(t2)
    lhu  s3, %lo(x)(t1)
    lh   s4, %lo(x)(t1)
    lbu  s5, %lo(x)(t1)
    lb   s6, %lo(x)(t1)
    sh   s0, %lo(y)(t2)
    sb   s0, %lo(y)(t2)
    lw   s7, %lo(y)(t2)
    mv   a0, t0
    li   a7, 34
    ecall
