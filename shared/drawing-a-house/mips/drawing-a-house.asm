# @screen unit=8 width=256 height=256 base=display
.eqv SIDE 32                # words across and down
.eqv SKY 0x0070B0E0
.eqv GRASS 0x003C9648
.eqv SUN 0x00FFD200
.eqv WALL 0x00C07040
.eqv ROOF 0x00A02020
.eqv DOOR 0x00704020

.data
display: .space 4096        # SIDE * SIDE words, four bytes each

.text
.globl main

# fill_rect(x, y, w, h): $a0-$a3 hold the dimensions;
# $s0 holds the colour and $s1 holds the grid base
fill_rect:
    blez $a2, rect_done     # nothing to draw when width or height is nonpositive
    blez $a3, rect_done
    move $t0, $a1           # row = y
    add $t1, $a1, $a3       # one past the last row
rect_rows:
    sll $t2, $t0, 5         # row * SIDE
    add $t2, $t2, $a0       # + x
    sll $t2, $t2, 2         # four bytes per word
    add $t2, $t2, $s1       # the first pixel of this run
    move $t3, $a2           # how many still to draw across
rect_cols:
    sw $s0, 0($t2)
    addi $t2, $t2, 4
    addi $t3, $t3, -1
    bnez $t3, rect_cols
    addi $t0, $t0, 1
    blt $t0, $t1, rect_rows
rect_done:
    jr $ra

# fill_disc(cx, cy, r): $a0-$a2 hold centre and radius;
# $s0 holds the colour and $s1 holds the grid base
fill_disc:
    mul $t7, $a2, $a2       # r * r
    sub $t0, $a1, $a2       # y = cy - r
    add $t1, $a1, $a2       # the last row
disc_rows:
    sub $t2, $a0, $a2       # x = cx - r
    add $t3, $a0, $a2
disc_cols:
    sub $t4, $t2, $a0       # dx
    sub $t5, $t0, $a1       # dy
    mul $t4, $t4, $t4
    mul $t5, $t5, $t5
    add $t4, $t4, $t5       # dx*dx + dy*dy
    bge $t4, $t7, disc_next
    sll $t6, $t0, 5         # the same address arithmetic again
    add $t6, $t6, $t2
    sll $t6, $t6, 2
    add $t6, $t6, $s1
    sw $s0, 0($t6)
disc_next:
    addi $t2, $t2, 1
    ble $t2, $t3, disc_cols
    addi $t0, $t0, 1
    ble $t0, $t1, disc_rows
    jr $ra

main:
    la $s1, display         # shared grid base for both helpers

    li $s0, SKY             # the sky, the top twenty rows
    li $a0, 0
    li $a1, 0
    li $a2, SIDE
    li $a3, 20
    jal fill_rect

    li $s0, GRASS           # the ground under it
    li $a0, 0
    li $a1, 20
    li $a2, SIDE
    li $a3, 12
    jal fill_rect

    li $s0, SUN             # a disc of radius 4, up in the corner
    li $a0, 26
    li $a1, 6
    li $a2, 4
    jal fill_disc

    li $s0, WALL            # the house
    li $a0, 10
    li $a1, 16
    li $a2, 12
    li $a3, 10
    jal fill_rect

    li $s0, ROOF            # six rows up from the wall, each two cells narrower
    li $s3, 0
roof:
    li $a0, 10
    add $a0, $a0, $s3
    li $a1, 15
    sub $a1, $a1, $s3
    li $a2, 12
    sll $t7, $s3, 1
    sub $a2, $a2, $t7
    li $a3, 1
    jal fill_rect
    addi $s3, $s3, 1
    blt $s3, 6, roof

    li $s0, DOOR            # and a door in the wall
    li $a0, 14
    li $a1, 21
    li $a2, 4
    li $a3, 5
    jal fill_rect

    li $v0, 10
    syscall
