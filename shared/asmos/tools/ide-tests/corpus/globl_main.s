# A .globl main starts the program wherever it is
.globl main
.text
helper:
    li   a0, 1
    li   a7, 1
    ecall
    ret
main:
    li   a0, 2
    li   a7, 1
    ecall
    jal  helper
