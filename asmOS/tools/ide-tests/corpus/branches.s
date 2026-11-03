# Every branch, taken and not taken, signed against unsigned
.text
main:
    li   s0, 0
    li   t0, -1
    li   t1, 1
    beq  t0, t1, no1
    addi s0, s0, 1
no1: bne  t0, t1, yes1
    addi s0, s0, 100
yes1: blt  t0, t1, yes2      # -1 < 1 signed
    addi s0, s0, 100
yes2: bltu t0, t1, no3      # 0xffffffff < 1 unsigned: no
    addi s0, s0, 2
no3: bge  t1, t0, yes4
    addi s0, s0, 100
yes4: bgeu t1, t0, no5
    addi s0, s0, 4
no5: bgeu t0, t0, yes6
    addi s0, s0, 100
yes6: bge  t0, t0, yes7
    addi s0, s0, 100
yes7: beq  zero, zero, yes8
    addi s0, s0, 100
yes8: blt  t1, t1, no9
    addi s0, s0, 8
no9: li   t2, 0x80000000
    li   t3, 0x7fffffff
    blt  t2, t3, yes10
    addi s0, s0, 100
yes10: bltu t3, t2, yes11
    addi s0, s0, 100
yes11: mv a0, s0
    li   a7, 1
    ecall
