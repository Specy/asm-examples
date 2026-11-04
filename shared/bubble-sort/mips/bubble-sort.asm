.eqv COUNT 8

.data
numbers: .word 42, 8, 15, 4, 23, 16, 99, 1

.text
main:
    li $t0, COUNT
    addi $t0, $t0, -1       # the outer loop runs COUNT-1 times
outer:
    la $t1, numbers         # back to the first element
    move $t2, $t0           # the inner loop is one shorter every pass
inner:
    lw $t3, 0($t1)          # the element we are on
    lw $t4, 4($t1)          # and the one after it
    slt $t5, $t4, $t3       # is right smaller than left?
    beqz $t5, in_order
    sw $t4, 0($t1)          # otherwise swap them
    sw $t3, 4($t1)
in_order:
    addi $t1, $t1, 4        # on to the next pair
    addi $t2, $t2, -1
    bnez $t2, inner
    addi $t0, $t0, -1
    bnez $t0, outer
