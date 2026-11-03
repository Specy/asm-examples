# print string from no memory stops the program, printing nothing
.text
main:
    li a0, 0x20000000
    li a7, 4
    ecall
