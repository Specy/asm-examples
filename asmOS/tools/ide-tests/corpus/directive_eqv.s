# .eqv names a number, a register or any text, before it is used
.eqv COUNT, 10
.eqv REG, t3
.eqv BASE 0x10010000
.eqv PRINT_INT 1
.eqv TWICE, COUNT
.data
array: .word COUNT, TWICE
.text
main:
    li   REG, COUNT
    addi REG, REG, COUNT
    li   s0, BASE
    lw   s1, 4(s0)
    mv   a0, REG
    li   a7, PRINT_INT
    ecall
