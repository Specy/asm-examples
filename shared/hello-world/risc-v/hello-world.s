.data
label: .asciz "Items: "

.text
.globl main
main:
    li a7, 4            # service 4: print a zero-terminated string
    la a0, label
    ecall

    li a7, 1            # service 1: print a signed integer
    li a0, 42
    ecall

    li a7, 11           # service 11: print one character
    li a0, '\n'
    ecall

    li a7, 10           # end this Playground run
    ecall
