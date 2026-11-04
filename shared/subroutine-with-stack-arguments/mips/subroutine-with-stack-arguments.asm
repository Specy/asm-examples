.text
.globl main

main:
    li $a0, 0                 # first four arguments are unused here
    li $a1, 0
    li $a2, 0
    li $a3, 0
    addi $sp, $sp, -8         # room for arguments five and six
    li $t0, 3
    sw $t0, 0($sp)            # fifth argument
    li $t0, 4
    sw $t0, 4($sp)            # sixth argument
    jal sum_of_squares
    addi $sp, $sp, 8          # caller releases its argument slots
    move $s1, $v0             # keep the answer before using $v0 for exit
    li $v0, 10
    syscall

# sum_of_squares(a, b, c, d, e, f): returns e*e + f*f in $v0
sum_of_squares:
    addi $sp, $sp, -12        # local, saved $fp, saved $ra
    sw $ra, 8($sp)
    sw $fp, 4($sp)
    addi $fp, $sp, 12         # points to the caller's fifth argument
    lw $a0, 0($fp)            # e
    jal square
    sw $v0, 0($sp)            # keep e*e across the next call
    lw $a0, 4($fp)            # f
    jal square
    lw $t0, 0($sp)
    add $v0, $v0, $t0         # e*e + f*f
    lw $ra, 8($sp)
    lw $fp, 4($sp)
    addi $sp, $sp, 12
    jr $ra

# square(x): x in $a0; result in $v0; restores the $s0 it borrows
square:
    addi $sp, $sp, -4
    sw $s0, 0($sp)
    move $s0, $a0
    mul $v0, $s0, $s0
    lw $s0, 0($sp)
    addi $sp, $sp, 4
    jr $ra
