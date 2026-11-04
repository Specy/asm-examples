# @screen unit=8 width=256 height=256 base=display
.eqv MMIO 0xffff0000
.eqv SIDE 32                # words across and down
.eqv BOX 3                  # the square, in words
.eqv LAST 29                # SIDE - BOX, the largest x or y
.eqv FRAME 80               # milliseconds per frame
.eqv BACKGROUND 0x00101820
.eqv BOXCOLOUR 0x0000C060

.data
display: .space 4096        # SIDE * SIDE words, four bytes each

.text
.globl main

# fill_rect(x, y, w, h): $a0-$a3 hold the rectangle;
# $s0 holds the colour and $s1 holds the grid base
fill_rect:
    move $t0, $a1           # row = y
    add $t1, $a1, $a3       # one past the last row
rect_rows:
    sll $t2, $t0, 5         # row * SIDE
    add $t2, $t2, $a0       # + x
    sll $t2, $t2, 2         # four bytes per word
    add $t2, $t2, $s1
    move $t3, $a2
rect_cols:
    sw $s0, 0($t2)
    addi $t2, $t2, 4
    addi $t3, $t3, -1
    bnez $t3, rect_cols
    addi $t0, $t0, 1
    blt $t0, $t1, rect_rows
    jr $ra

main:
    la $s1, display
    li $s7, MMIO
    li $s0, BACKGROUND      # paint the grid once
    li $a0, 0
    li $a1, 0
    li $a2, SIDE
    li $a3, SIDE
    jal fill_rect

    li $s2, 14              # x
    li $s3, 14              # y
    li $s4, 1               # dx
    li $s5, 0               # dy

frame:
    li $s0, BACKGROUND      # erase the square where it was
    move $a0, $s2
    move $a1, $s3
    li $a2, BOX
    li $a3, BOX
    jal fill_rect

# --- one key sets the direction, it does not move the square -----------------
    lw $t4, 0($s7)          # the receiver control register
    andi $t4, $t4, 1        # the Ready bit
    beqz $t4, no_key
    lw $t5, 4($s7)          # read the waiting character
    andi $t5, $t5, 0xFF
    bne $t5, 'a', not_a
    li $s4, -1
    li $s5, 0
not_a:
    bne $t5, 'd', not_d
    li $s4, 1
    li $s5, 0
not_d:
    bne $t5, 'w', not_w
    li $s4, 0
    li $s5, -1
not_w:
    bne $t5, 's', no_key
    li $s4, 0
    li $s5, 1
no_key:

# --- and the square moves on its own, coming back in at the far edge ---------
    add $s2, $s2, $s4
    ble $s2, LAST, x_low
    li $s2, 0               # off the right edge, back at the left
x_low:
    bgez $s2, x_done
    li $s2, LAST            # off the left edge, back at the right
x_done:
    add $s3, $s3, $s5
    ble $s3, LAST, y_low
    li $s3, 0
y_low:
    bgez $s3, y_done
    li $s3, LAST
y_done:

    li $s0, BOXCOLOUR       # and draw it where it is now
    move $a0, $s2
    move $a1, $s3
    li $a2, BOX
    li $a3, BOX
    jal fill_rect

    li $v0, 32              # a frame of program time
    li $a0, FRAME
    syscall
    j frame
