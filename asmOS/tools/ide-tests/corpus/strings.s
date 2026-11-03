# String handling: length, upper case and reversal in place
.data
text: .asciz "Hello, RISC-V world!"
nl:   .asciz "\n"
.text
main:
    la   a0, text
    jal  strlen
    mv   s0, a0
    li   a7, 1
    ecall
    la   a0, nl
    li   a7, 4
    ecall
    la   t0, text
upper:
    lbu  t1, 0(t0)
    beqz t1, upper_done
    li   t2, 'a'
    blt  t1, t2, next_char
    li   t2, 'z'
    bgt  t1, t2, next_char
    addi t1, t1, -32
    sb   t1, 0(t0)
next_char:
    addi t0, t0, 1
    j    upper
upper_done:
    la   t0, text
    add  t1, t0, s0
    addi t1, t1, -1
reverse:
    bge  t0, t1, reversed
    lbu  t2, 0(t0)
    lbu  t3, 0(t1)
    sb   t3, 0(t0)
    sb   t2, 0(t1)
    addi t0, t0, 1
    addi t1, t1, -1
    j    reverse
reversed:
    la   a0, text
    li   a7, 4
    ecall
    li   a7, 10
    ecall
strlen:
    mv   t0, a0
strlen_loop:
    lbu  t1, 0(t0)
    beqz t1, strlen_done
    addi t0, t0, 1
    j    strlen_loop
strlen_done:
    sub  a0, t0, a0
    ret
