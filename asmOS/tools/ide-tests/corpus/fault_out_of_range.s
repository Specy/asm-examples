# Address 0 is no memory at all
.text
main:
    li  s0, 3
    lw  t0, 0(zero)
