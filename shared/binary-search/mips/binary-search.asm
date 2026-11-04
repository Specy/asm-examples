.eqv COUNT 12

.data
numbers: .word 2, 5, 8, 12, 16, 23, 38, 56, 72, 91, 100, 127

.text
main:
    la $t0, numbers         # base address of the array
    li $t1, 91              # target
    li $t2, 0               # low = first index
    li $t3, COUNT
    addi $t3, $t3, -1       # high = last index
    li $t4, -1              # result if the target is absent

search:
    bgt $t2, $t3, search_done  # low > high: no candidates remain
    add $t5, $t2, $t3
    srl $t5, $t5, 1         # mid = (low + high) / 2, rounded down
    sll $t6, $t5, 2         # byte offset = mid * 4
    add $t6, $t0, $t6       # address of numbers[mid]
    lw $t7, 0($t6)          # middle value
    beq $t7, $t1, found
    bgt $t7, $t1, middle_too_big
    addi $t2, $t5, 1        # middle value is too small: keep the right half
    j search

middle_too_big:
    addi $t3, $t5, -1       # middle value is too big: keep the left half
    j search

found:
    move $t4, $t5           # save the matching index

search_done:
    li $v0, 10
    syscall
