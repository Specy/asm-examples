.data
numbers: .word 4, 8, 15, 16, 23, 42
end:

.text
main:
    la $t0, numbers     # address of the next element
    la $t1, end         # address one past the array
    li $t2, 0           # running total
loop:
    beq $t0, $t1, done  # check before loading, so an empty array is valid
    lw $t3, 0($t0)      # load the element under the pointer
    addu $t2, $t2, $t3  # add this word to the total
    addiu $t0, $t0, 4   # advance to the next word
    j loop
done:
    li $v0, 10          # exit
    syscall
