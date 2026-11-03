# A halfword load from an odd address
.data
x: .word 1
.text
main:
    la  t0, x
    lh  t1, 1(t0)
