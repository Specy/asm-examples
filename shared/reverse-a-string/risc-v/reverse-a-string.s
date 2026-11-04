.data
text: .asciz "Assembly"

.text
main:
    la t0, text         # the left pointer
    mv t1, t0           # the right one, still at the start
find_end:
    lb t2, 0(t1)
    beqz t2, at_end     # walk it to the terminator
    addi t1, t1, 1
    j find_end
at_end:
    beq t1, t0, done    # an empty string has no last character
    addi t1, t1, -1     # back onto the last character
swap_loop:
    bgeu t0, t1, done   # stop once they have met or crossed
    lb t2, 0(t0)
    lb t3, 0(t1)
    sb t3, 0(t0)        # each byte into the other's place
    sb t2, 0(t1)
    addi t0, t0, 1
    addi t1, t1, -1
    j swap_loop
done:
