# A negative sbrk stops the program
.text
main:
    li a0, -4
    li a7, 9
    ecall
