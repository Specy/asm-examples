# java.util.Random, exactly: seeded generators, and one seeded by the host
.text
main:
    li   a0, 0
    li   a1, 12345
    li   a7, 40
    ecall
    li   s0, 0
loop:
    li   a0, 0
    li   a7, 41
    ecall
    li   a7, 1
    ecall
    li   a0, ' '
    li   a7, 11
    ecall
    li   a0, 0
    li   a1, 100
    li   a7, 42
    ecall
    li   a7, 1
    ecall
    li   a0, ' '
    li   a7, 11
    ecall
    addi s0, s0, 1
    li   t0, 5
    blt  s0, t0, loop
    li   a0, 3            # generator 3, seeded -1
    li   a1, -1
    li   a7, 40
    ecall
    li   a0, 3
    li   a1, 1000000
    li   a7, 42
    ecall
    li   a7, 1
    ecall
    li   a0, ' '
    li   a7, 11
    ecall
    li   a0, 7            # never seeded: the host's seed, 42
    li   a1, 77
    li   a7, 42
    ecall
    li   a7, 1
    ecall
    li   a0, 7
    li   a7, 41
    ecall
    li   a7, 1
    ecall
