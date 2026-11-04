.data
numbers: .word 4, 8, 15, 16, 23, 42
end:

.text
main:
    la t0, numbers      # the first element
    la t1, end          # one word past the last
    li t2, 0            # the running total
loop:
    lw t3, 0(t0)        # read the element t0 points at
    add t2, t2, t3
    addi t0, t0, 4      # step to the next word
    bne t0, t1, loop
