# @screen unit=16 width=512 height=512 base=display
.eqv MMIO, 0xffff0000
.eqv SIDE, 32
.eqv LAST, 31
.eqv CELLS, 1024
.eqv MAXLEN, 64
.eqv FRAME, 120
.eqv BACKGROUND, 0x00101820
.eqv SNAKE, 0x0040D040
.eqv FOOD, 0x00FF4000
.eqv DEAD, 0x00400810

.data
display: .space 4096
body:    .word 0x050C, 0x040C, 0x030C
         .space 244         # room for MAXLEN segments in all
length:  .word 3
dx:      .word 1
dy:      .word 0
score:   .word 0
seed:    .word 0x1F123BB5
food:    .word 0x140C       # column 20, row 12
label:   .asciz "Score: "
over:    .asciz "Game over. Score: "

.text
.globl main
main:
    addi sp, sp, -12        # align the Playground stack before calls
    la s0, display
    li s2, MMIO
    li s5, 0
    li s7, LAST
    li s8, 'a'
    li s9, 'd'
    li s10, 'w'
    li s11, 's'

    li s1, BACKGROUND
    jal fill_grid
    li s1, SNAKE
    la t2, body
    lw t3, length
start_body:
    lw a0, 0(t2)
    jal draw_cell
    addi t2, t2, 4
    addi t3, t3, -1
    bnez t3, start_body
    li s1, FOOD
    lw a0, food
    jal draw_cell

frame:
    lw t0, 0(s2)            # receiver control register
    andi t0, t0, 1          # Ready bit
    beqz t0, no_key
    lw t1, 4(s2)            # reading receiver data takes the character
    andi t1, t1, 0xFF
    bne t1, s8, not_a
    li a0, -1
    li a1, 0
    jal try_direction
not_a:
    bne t1, s9, not_d
    li a0, 1
    li a1, 0
    jal try_direction
not_d:
    bne t1, s10, not_w
    li a0, 0
    li a1, -1
    jal try_direction
not_w:
    bne t1, s11, no_key
    li a0, 0
    li a1, 1
    jal try_direction
no_key:

    lw t0, length
    slli t0, t0, 2
    la t1, body
    add t1, t1, t0
    addi t1, t1, -4         # &body[length - 1]
    lw s3, 0(t1)            # old tail cell
    lw t2, length
    addi t2, t2, -1
    beqz t2, moved
shift:
    lw t3, -4(t1)           # body[i] = body[i - 1]
    sw t3, 0(t1)
    addi t1, t1, -4
    addi t2, t2, -1
    bnez t2, shift
moved:

    lw t0, body             # old head: x in high byte, y in low byte
    srli t1, t0, 8
    andi t2, t0, 0xFF
    lw t3, dx
    add t1, t1, t3
    lw t3, dy
    add t2, t2, t3
    bltz t1, game_over
    bgt t1, s7, game_over
    bltz t2, game_over
    bgt t2, s7, game_over
    slli t1, t1, 8
    or s4, t1, t2
    la t0, body
    sw s4, 0(t0)

    la t1, body
    addi t1, t1, 4
    lw t2, length
    addi t2, t2, -1
    beqz t2, no_bite
bite:
    lw t3, 0(t1)
    beq t3, s4, game_over
    addi t1, t1, 4
    addi t2, t2, -1
    bnez t2, bite
no_bite:

    lw t3, food
    bne t3, s4, no_meal
    lw t4, score
    addi t4, t4, 1
    la t0, score
    sw t4, 0(t0)
    lw t4, length
    li t5, MAXLEN
    bge t4, t5, no_room
    slli t5, t4, 2
    la t6, body
    add t6, t6, t5
    sw s3, 0(t6)
    addi t4, t4, 1
    la t0, length
    sw t4, 0(t0)
    li s5, 1                # skip the tail erase once
no_room:
    jal place_food
    jal print_score
no_meal:

    beqz s5, erase_tail
    li s5, 0
    j tail_done
erase_tail:
    li s1, BACKGROUND
    mv a0, s3
    jal draw_cell
tail_done:
    li s1, SNAKE
    mv a0, s4
    jal draw_cell
    li s1, FOOD
    lw a0, food
    jal draw_cell

    li a7, 32
    li a0, FRAME
    ecall
    j frame

game_over:
    li s1, DEAD
    jal fill_grid
    li a7, 4
    la a0, over
    ecall
    li a7, 1
    lw a0, score
    ecall
    li a7, 11
    li a0, '\n'
    ecall
    li a7, 10
    ecall

# fill_grid(): paint every grid word with s1.
# Reads s0 and s1; changes only t0 and t1.
fill_grid:
    mv t0, s0
    li t1, CELLS
fill_next:
    sw s1, 0(t0)
    addi t0, t0, 4
    addi t1, t1, -1
    bnez t1, fill_next
    ret

# draw_cell(c): paint packed cell a0 with colour s1.
# Reads s0 and s1; changes only t0 and t1.
draw_cell:
    srli t0, a0, 8
    andi t1, a0, 0xFF
    slli t1, t1, 5
    add t0, t0, t1
    slli t0, t0, 2
    add t0, t0, s0
    sw s1, 0(t0)
    ret

# try_direction(nx, ny): use a0,a1 unless that direction is directly backward.
# Changes only t6.
try_direction:
    lw t6, dx
    add t6, t6, a0
    bnez t6, take_it
    lw t6, dy
    add t6, t6, a1
    beqz t6, no_turn
take_it:
    la t6, dx
    sw a0, 0(t6)
    la t6, dy
    sw a1, 0(t6)
no_turn:
    ret

# place_food(): choose a deterministic free cell and store it in food.
# Calls next_random; changes a0 and t0-t6.
place_food:
    addi sp, sp, -16
    sw ra, 0(sp)
food_retry:
    jal next_random
    andi t0, a0, LAST
    slli t0, t0, 8
    jal next_random
    andi t1, a0, LAST
    or t0, t0, t1
    la t2, body
    lw t3, length
food_scan:
    lw t4, 0(t2)
    beq t4, t0, food_retry
    addi t2, t2, 4
    addi t3, t3, -1
    bnez t3, food_scan
    la t2, food
    sw t0, 0(t2)
    lw ra, 0(sp)
    addi sp, sp, 16
    ret

# next_random(): update seed and return the next deterministic value in a0.
# Changes only a0 and t6.
next_random:
    lw a0, seed
    slli t6, a0, 13
    xor a0, a0, t6
    srli t6, a0, 17
    xor a0, a0, t6
    slli t6, a0, 5
    xor a0, a0, t6
    la t6, seed
    sw a0, 0(t6)
    ret

# print_score(): print the label, score, and newline; changes a0 and a7.
print_score:
    li a7, 4
    la a0, label
    ecall
    li a7, 1
    lw a0, score
    ecall
    li a7, 11
    li a0, '\n'
    ecall
    ret
