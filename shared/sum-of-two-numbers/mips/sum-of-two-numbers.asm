.data
first:  .asciiz "First number: "
second: .asciiz "\nSecond number: "
answer: .asciiz "\nThe sum is "

.text
.globl main
main:
    li $v0, 4
    la $a0, first
    syscall
    li $v0, 5           # service 5: read an integer into $v0
    syscall
    move $t0, $v0       # keep the first number before reusing $v0

    li $v0, 4
    la $a0, second
    syscall
    li $v0, 5
    syscall
    add $t0, $t0, $v0   # use the second number before reusing $v0

    li $v0, 4
    la $a0, answer
    syscall
    li $v0, 1           # service 1: print a signed number
    move $a0, $t0
    syscall

    li $v0, 10
    syscall
