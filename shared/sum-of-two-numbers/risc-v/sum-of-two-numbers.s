.data
first:  .asciz "First number: "
second: .asciz "\nSecond number: "
answer: .asciz "\nThe sum is "

.text
.globl main
main:
    li a7, 4
    la a0, first
    ecall
    li a7, 5            # service 5: read an integer into a0
    ecall
    mv t0, a0           # keep the first number

    li a7, 4
    la a0, second
    ecall
    li a7, 5
    ecall
    add t0, t0, a0      # add the second number

    li a7, 4
    la a0, answer
    ecall
    li a7, 1            # service 1: print it as a signed number
    mv a0, t0
    ecall

    li a7, 10
    ecall
