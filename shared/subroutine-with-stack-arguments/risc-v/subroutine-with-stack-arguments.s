.text
.globl main

# square(x): x arrives in a0 and its square leaves in a0.
# This frame exists only because this version deliberately uses s1.
square:
    addi sp, sp, -16
    sw s1, 0(sp)            # preserve the caller's callee-saved s1
    mv s1, a0
    mul a0, s1, s1
    lw s1, 0(sp)
    addi sp, sp, 16
    ret

# Teaching convention: a is at 0(fp), b is at 4(fp); result leaves in a0.
sum_of_squares:
    addi sp, sp, -16        # local at 0(sp), saved fp and ra above it
    sw fp, 8(sp)
    sw ra, 12(sp)           # nested jal instructions will overwrite ra
    addi fp, sp, 16         # fp (s0) now points to the caller's arguments

    lw a0, 0(fp)
    jal square
    sw a0, 0(sp)            # retain a * a across the next call
    lw a0, 4(fp)
    jal square
    lw t0, 0(sp)
    add a0, a0, t0

    lw fp, 8(sp)
    lw ra, 12(sp)
    addi sp, sp, 16
    ret

main:
    li s0, 123              # sentinels prove that both saved registers come back unchanged
    li s1, 456
    addi sp, sp, -12        # 0x7fffeffc -> 0x7fffeff0, aligned for jal
    li t0, 3
    sw t0, 0(sp)            # a
    li t0, 4
    sw t0, 4(sp)            # b
    jal sum_of_squares
    addi sp, sp, 12         # give back the two arguments and four padding bytes
    mv s2, a0               # 25; s0 is 123 and s1 is 456 again
