.eqv COUNT, 12

.data
numbers: .word 2, 5, 8, 12, 16, 23, 38, 56, 72, 91, 100, 127

.text
main:
    la t0, numbers          # base address of the array
    li t1, 91               # target
    li t2, 0                # low = 0
    li t3, COUNT
    addi t3, t3, -1         # high = COUNT - 1
    li t4, -1               # answer = -1, meaning absent
search:
    bgt t2, t3, search_done # while (low <= high)
    add t5, t2, t3
    srli t5, t5, 1          # mid = (low + high) / 2
    slli t6, t5, 2          # byte offset = mid * 4
    add t6, t0, t6          # address of numbers[mid]
    lw t6, 0(t6)            # value = numbers[mid]
    beq t6, t1, found
    bgt t6, t1, too_big
    addi t2, t5, 1          # value < target: low = mid + 1
    j search
too_big:
    addi t3, t5, -1         # value > target: high = mid - 1
    j search
found:
    mv t4, t5               # answer = mid
search_done:
