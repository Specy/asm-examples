# @screen unit=8 width=256 height=256 base=display
.eqv SIDE, 32
.eqv SKY, 0x0070B0E0
.eqv GRASS, 0x003C9648
.eqv SUN, 0x00FFD200
.eqv WALL, 0x00C07040
.eqv ROOF, 0x00A02020
.eqv DOOR, 0x00704020

.data
display: .space 4096        # 32 * 32 words, four bytes each

.text
.globl main

# a0=x, a1=y, a2=width, a3=height
# Reads s0=display base and s1=colour. Fixed for a 32-cell-wide display.
fill_rect_32:
    blez a2, rect_done
    blez a3, rect_done
    mv t0, a1               # current row
    add t1, a1, a3          # one-past the final row
rect_rows:
    slli t2, t0, 5          # row * 32
    add t2, t2, a0          # + x
    slli t2, t2, 2          # four bytes per word
    add t2, t2, s0          # first cell in this row
    mv t3, a2               # cells left across the row
rect_cols:
    sw s1, 0(t2)
    addi t2, t2, 4
    addi t3, t3, -1
    bnez t3, rect_cols
    addi t0, t0, 1
    blt t0, t1, rect_rows
rect_done:
    ret

# a0=cx, a1=cy, a2=radius
# Reads s0=display base and s1=colour. Fixed for a 32-cell-wide display.
fill_disc_32:
    mul t6, a2, a2          # radius squared
    sub t0, a1, a2          # first y: cy - radius
    add t1, a1, a2          # final y: cy + radius
disc_rows:
    sub t2, a0, a2          # first x: cx - radius
    add t3, a0, a2          # final x: cx + radius
disc_cols:
    sub t4, t2, a0          # dx
    sub t5, t0, a1          # dy
    mul t4, t4, t4
    mul t5, t5, t5
    add t4, t4, t5
    bge t4, t6, disc_next   # outside the disc: do not store
    slli t4, t0, 5
    add t4, t4, t2
    slli t4, t4, 2
    add t4, t4, s0
    sw s1, 0(t4)
disc_next:
    addi t2, t2, 1
    ble t2, t3, disc_cols
    addi t0, t0, 1
    ble t0, t1, disc_rows
    ret

main:
    la s0, display

    li s1, SKY
    li a0, 0
    li a1, 0
    li a2, SIDE
    li a3, 20
    jal fill_rect_32

    li s1, GRASS
    li a0, 0
    li a1, 20
    li a2, SIDE
    li a3, 12
    jal fill_rect_32

    li s1, SUN
    li a0, 26
    li a1, 6
    li a2, 4
    jal fill_disc_32

    li s1, WALL
    li a0, 10
    li a1, 16
    li a2, 12
    li a3, 10
    jal fill_rect_32

    li s1, ROOF
    li s2, 0                # roof row: 0 through 5
    li s3, 6
roof:
    li a0, 10
    add a0, a0, s2
    li a1, 15
    sub a1, a1, s2
    li a2, 12
    slli t0, s2, 1
    sub a2, a2, t0
    li a3, 1
    jal fill_rect_32
    addi s2, s2, 1
    blt s2, s3, roof

    li s1, DOOR
    li a0, 14
    li a1, 21
    li a2, 4
    li a3, 5
    jal fill_rect_32

    li a7, 10
    ecall
