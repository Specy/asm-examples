# Bubble sort of a word array, then printed
.data
array: .word 34, -7, 0, 2147483647, 12, -2147483648, 5, 5, 99, -1
count: .word 10
space: .asciz " "
.text
main:
    la   s0, array
    lw   s1, count
outer:
    li   t0, 0
    li   t1, 1
    addi t2, s1, -1
inner:
    bge  t0, t2, pass_done
    slli t3, t0, 2
    add  t3, s0, t3
    lw   t4, 0(t3)
    lw   t5, 4(t3)
    ble  t4, t5, in_order
    sw   t5, 0(t3)
    sw   t4, 4(t3)
    li   t1, 0
in_order:
    addi t0, t0, 1
    j    inner
pass_done:
    beqz t1, outer
    li   t0, 0
print:
    slli t3, t0, 2
    add  t3, s0, t3
    lw   a0, 0(t3)
    li   a7, 1
    ecall
    la   a0, space
    li   a7, 4
    ecall
    addi t0, t0, 1
    blt  t0, s1, print
