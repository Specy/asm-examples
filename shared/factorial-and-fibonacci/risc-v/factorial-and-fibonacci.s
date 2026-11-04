.text
.globl main

# factorial(n), for nonnegative n: n in a0, answer in a0
factorial:
    addi sp, sp, -16
    sw ra, 4(sp)            # this call's return address
    sw a0, 0(sp)            # this call's n
    li t0, 2
    blt a0, t0, fact_one    # factorial(0) and factorial(1) are 1
    addi a0, a0, -1
    jal factorial           # a0 = factorial(n - 1)
    lw t1, 0(sp)            # recover this call's n
    mul a0, a0, t1          # n * factorial(n - 1)
    j fact_done
fact_one:
    li a0, 1
fact_done:
    lw ra, 4(sp)
    addi sp, sp, 16
    ret

# fib(n), for nonnegative n: n in a0, answer in a0
fib:
    addi sp, sp, -16
    sw ra, 4(sp)
    sw a0, 0(sp)
    li t0, 2
    blt a0, t0, fib_done    # fib(0) is 0 and fib(1) is 1
    addi a0, a0, -1
    jal fib                 # a0 = fib(n - 1)
    sw a0, 8(sp)            # keep the first result across the second call
    lw a0, 0(sp)
    addi a0, a0, -2
    jal fib                 # a0 = fib(n - 2)
    lw t1, 8(sp)            # recover fib(n - 1)
    add a0, a0, t1          # fib(n - 2) + fib(n - 1)
fib_done:
    lw ra, 4(sp)
    addi sp, sp, 16
    ret

main:
    addi sp, sp, -12        # 0x7fffeffc -> 0x7fffeff0, aligned for calls
    li a0, 8
    jal factorial
    mv s0, a0               # 40320
    li a0, 10
    jal fib
    mv s1, a0               # 55
    addi sp, sp, 12         # restore the Playground's initial sp
