# @test keys=Hello,\nworld.
# The keyboard receiver, polled with a sleep between polls, echoed through the display register
.text
main:
    li   s0, 0xffff0000
poll:
    lw   t0, 0(s0)         # receiver control
    andi t0, t0, 1
    bnez t0, take
    li   a7, 32
    li   a0, 10
    ecall
    j    poll
take:
    lw   t1, 4(s0)         # receiver data
wait:
    lw   t2, 8(s0)         # transmitter control: always ready
    andi t2, t2, 1
    beqz t2, wait
    sw   t1, 12(s0)
    li   t3, '.'
    bne  t1, t3, poll
    lw   s1, 0(s0)
    lw   s2, 4(s0)
