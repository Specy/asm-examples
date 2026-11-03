# A halfword store to an odd address
.data
x: .word 1, 2
.text
main:
    la  t0, x
    sh  t0, 3(t0)
