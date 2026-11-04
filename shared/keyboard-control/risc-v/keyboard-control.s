# @screen unit=8 width=256 height=256 base=display
.eqv MMIO, 0xffff0000
.eqv SIDE, 32               # words across and down
.eqv BOX, 3                 # the square, in words
.eqv LAST, 29               # SIDE - BOX, the largest x or y
.eqv FRAME, 80              # milliseconds per frame
.eqv BACKGROUND, 0x00101820
.eqv BOXCOLOUR, 0x0000C060

.data
display: .space 4096        # SIDE * SIDE words, four bytes each

.text
.globl main

# fill_rect(x, y, width, height)
# Reads s0=display base and s1=colour; changes only t0-t3.
fill_rect:
    blez a2, rect_done
    blez a3, rect_done
    mv t0, a1               # row = y
    add t1, a1, a3          # one past the last row
rect_rows:
    slli t2, t0, 5          # row * SIDE
    add t2, t2, a0          # + x
    slli t2, t2, 2          # four bytes per word
    add t2, t2, s0
    mv t3, a2
rect_cols:
    sw s1, 0(t2)
    addi t2, t2, 4
    addi t3, t3, -1
    bnez t3, rect_cols
    addi t0, t0, 1
    blt t0, t1, rect_rows
rect_done:
    ret

main:
    addi sp, sp, -12        # align the Playground stack before calls
    la s0, display
    li s6, MMIO
    li s7, LAST             # largest top-left x or y
    li s8, 'a'              # fixed key codes for branch comparisons
    li s9, 'd'
    li s10, 'w'
    li s11, 's'

    li s1, BACKGROUND       # paint the grid once
    li a0, 0
    li a1, 0
    li a2, SIDE
    li a3, SIDE
    jal fill_rect

    li s2, 14               # x
    li s3, 14               # y
    li s4, 1                # dx
    li s5, 0                # dy

frame:
    li s1, BACKGROUND       # erase the square where it was
    mv a0, s2
    mv a1, s3
    li a2, BOX
    li a3, BOX
    jal fill_rect

# --- one key sets the direction, it does not move the square -----------------
    lw t4, 0(s6)            # the receiver control register
    andi t4, t4, 1          # the Ready bit
    beqz t4, input_done
    lw t5, 4(s6)            # the receiver data, which takes the character
    andi t5, t5, 0xFF
    bne t5, s8, not_a
    li s4, -1
    li s5, 0
    j input_done
not_a:
    bne t5, s9, not_d
    li s4, 1
    li s5, 0
    j input_done
not_d:
    bne t5, s10, not_w
    li s4, 0
    li s5, -1
    j input_done
not_w:
    bne t5, s11, input_done
    li s4, 0
    li s5, 1
input_done:

# --- and the square moves on its own, coming back in at the far edge ---------
    add s2, s2, s4
    ble s2, s7, x_low
    li s2, 0                # off the right edge, back at the left
x_low:
    bgez s2, x_done
    li s2, LAST             # off the left edge, back at the right
x_done:
    add s3, s3, s5
    ble s3, s7, y_low
    li s3, 0
y_low:
    bgez s3, y_done
    li s3, LAST
y_done:

    li s1, BOXCOLOUR        # and draw it where it is now
    mv a0, s2
    mv a1, s3
    li a2, BOX
    li a3, BOX
    jal fill_rect

    li a7, 32               # wait one frame
    li a0, FRAME
    ecall
    j frame
