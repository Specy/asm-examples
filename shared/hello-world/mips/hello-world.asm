.data
greeting: .asciiz "Hello, world!\n"
question: .asciiz "The answer is "

.text
.globl main
main:
    li $v0, 4           # service 4: print a null terminated string
    la $a0, greeting
    syscall

    li $v0, 4
    la $a0, question
    syscall
    li $v0, 1           # service 1: print a signed integer
    li $a0, 42
    syscall
    li $v0, 11          # service 11: print one character
    li $a0, '\n'
    syscall

    li $v0, 10          # service 10: end the program
    syscall
