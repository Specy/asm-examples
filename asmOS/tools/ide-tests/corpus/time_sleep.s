# Program time moves with sleep (the tests use a virtual clock)
.text
main:
    li   a7, 30
    ecall
    mv   s0, a0
    mv   s1, a1
    li   a0, 250
    li   a7, 32
    ecall
    li   a7, 30
    ecall
    sub  a0, a0, s0
    li   a7, 1
    ecall
    li   a0, ' '
    li   a7, 11
    ecall
    mv   a0, s1
    li   a7, 1
    ecall
    li   a0, 60
    li   a1, 100
    li   a2, 0
    li   a3, 100
    li   a7, 33            # MIDI out synchronous: sleeps as long as the note
    ecall
    li   a7, 30
    ecall
    sub  a0, a0, s0
    li   a7, 1
    ecall
    li   a0, 72
    li   a1, -5
    li   a2, 200
    li   a3, -3
    li   a7, 31
    ecall
