.data
buffer:     .space 34       # room for 31 binary digits, a zero, and two spare bytes
buffer_end:
newline:    .asciiz "\n"

.text
.globl main

# print_in_base(n, base): n in $a0, base in $a1
print_in_base:
    la $t0, buffer_end      # build the text backwards from the end
    addi $t0, $t0, -1
    sb $zero, 0($t0)        # the terminator goes down first
digit:
    div $a0, $a1            # n / base in lo, n % base in hi
    mfhi $t1                # the digit
    mflo $a0                # n = n / base
    blt $t1, 10, number
    addi $t1, $t1, 55       # 'A' is 65, so a 10 becomes 'A'
    j store
number:
    addi $t1, $t1, 48       # '0' is 48, so a 0 becomes '0'
store:
    addi $t0, $t0, -1       # next free byte before the digits already stored
    sb $t1, 0($t0)
    bnez $a0, digit         # until nothing is left of n
    li $v0, 4               # service 4: print the string that was built
    move $a0, $t0
    syscall
    li $v0, 4
    la $a0, newline
    syscall
    jr $ra

main:
    li $a0, 48879
    li $a1, 16              # in hexadecimal
    jal print_in_base
    li $a0, 48879
    li $a1, 10              # in decimal
    jal print_in_base
    li $a0, 48879
    li $a1, 2               # in binary
    jal print_in_base
    li $v0, 10
    syscall
