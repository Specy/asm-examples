# .data with an address
.data 0x10000000
low: .word 0x11111111
.data 0x10020000
high: .word 0x22222222
.data
next: .word 0x33333333
.text
main:
    la s0, low
    la s1, high
    la s2, next
    lw s3, 0(s0)
    lw s4, 0(s1)
    lw s5, 0(s2)
    mv a0, s2
    li a7, 34
    ecall
