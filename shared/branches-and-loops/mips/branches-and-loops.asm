.eqv COUNT 10

.data
numbers: .space 40      # room for ten 4-byte words

.text
.globl main
main:
# If and else: the maximum in $t2, the distance in $t4
    li $t0, 37          # a = 37
    li $t1, 64          # b = 64

    slt $t3, $t0, $t1   # $t3 = 1 when signed a < signed b
    beqz $t3, a_is_at_least_b

    move $t2, $t1       # maximum = b
    subu $t4, $t1, $t0  # distance = b - a
    j compared

a_is_at_least_b:
    move $t2, $t0       # maximum = a
    subu $t4, $t0, $t1  # distance = a - b

compared:
# A loop: the numbers 1 to 10 into the words at numbers
    la $t5, numbers     # address for the next store
    li $t6, 1           # next value to store
    li $t7, COUNT       # stores remaining

    beq $t7, $zero, done

fill:
    sw $t6, 0($t5)
    addi $t5, $t5, 4
    addi $t6, $t6, 1
    addi $t7, $t7, -1
    bne $t7, $zero, fill

done:
    li $v0, 10
    syscall
