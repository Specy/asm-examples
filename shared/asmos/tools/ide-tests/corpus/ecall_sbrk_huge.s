# More heap than the data segment holds stops the program
.text
main:
    li a0, 0x00400000
    li a7, 9
    ecall
