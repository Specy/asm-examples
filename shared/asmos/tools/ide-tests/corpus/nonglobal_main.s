# Without .globl, main is an ordinary label: the program starts at the first instruction
.text
helper:
    li   a0, 1
    li   a7, 1
    ecall
main:
    li   a0, 2
    li   a7, 1
    ecall
