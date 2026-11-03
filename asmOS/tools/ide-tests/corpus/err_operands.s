# Too few and too many operands
.text
main:
    add t0, t1
    nop t0
    lw  t0
    beq t0, t1
    ecall t0
    ret t0
    li  t0, 1, 2
