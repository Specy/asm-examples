.data
text: .asciiz "Assembly"

.text
main:
    la $t0, text             # left pointer: first byte
    addu $t1, $t0, $zero    # scan pointer: start at the same byte

find_end:
    lbu $t2, 0($t1)
    beq $t2, $zero, found_end
    addiu $t1, $t1, 1
    j find_end

found_end:
    beq $t1, $t0, done      # empty string: no last letter to move back to
    addiu $t1, $t1, -1      # right pointer: last letter, before the zero

swap_loop:
    sltu $t4, $t0, $t1      # 1 while the left address is below the right
    beq $t4, $zero, done
    lbu $t2, 0($t0)         # save both bytes before either store
    lbu $t3, 0($t1)
    sb $t3, 0($t0)
    sb $t2, 0($t1)
    addiu $t0, $t0, 1
    addiu $t1, $t1, -1
    j swap_loop

done:
    addiu $v0, $zero, 10
    syscall
