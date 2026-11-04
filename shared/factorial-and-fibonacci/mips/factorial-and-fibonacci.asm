.text
.globl main

# factorial(n): n in $a0, the answer in $v0
factorial:
    addi $sp, $sp, -8
    sw $ra, 4($sp)          # this call's return address
    sw $a0, 0($sp)          # this call's n
    blt $a0, 2, fact_one    # 0! and 1! are 1
    addi $a0, $a0, -1
    jal factorial           # factorial(n - 1)
    lw $a0, 0($sp)          # recover this call's n
    mul $v0, $v0, $a0       # n * factorial(n - 1)
    j fact_done
fact_one:
    li $v0, 1
fact_done:
    lw $ra, 4($sp)
    addi $sp, $sp, 8
    jr $ra

# fib(n): n in $a0, the answer in $v0
fib:
    addi $sp, $sp, -12
    sw $ra, 8($sp)
    sw $a0, 4($sp)
    blt $a0, 2, fib_small   # fib(0) is 0 and fib(1) is 1
    addi $a0, $a0, -1
    jal fib                 # fib(n - 1)
    sw $v0, 0($sp)          # keep the first result through the next call
    lw $a0, 4($sp)
    addi $a0, $a0, -2
    jal fib                 # fib(n - 2)
    lw $t0, 0($sp)
    add $v0, $v0, $t0       # fib(n - 1) + fib(n - 2)
    j fib_done
fib_small:
    move $v0, $a0
fib_done:
    lw $ra, 8($sp)
    addi $sp, $sp, 12
    jr $ra

main:
    li $a0, 8
    jal factorial
    move $s0, $v0           # 8!
    li $a0, 10
    jal fib
    move $s1, $v0           # fib(10)
    li $v0, 10              # exit after saving both answers
    syscall
