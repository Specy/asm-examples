# A word load from an address that is not a multiple of four
.data
x: .word 1, 2
.text
main:
    la  t0, x
    li  s0, 1
    lw  a0, 1(t0)
    li  s1, 2
