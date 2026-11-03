# read integer refuses what is not one
.text
main:
    li a7, 5
    ecall
    li a7, 1
    ecall
    li a7, 5
    ecall
