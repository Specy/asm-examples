# A .word too large for 32 bits is cut to its low bits, with a warning
.data
a: .word 4294967295
b: .word 0x100000005
c: .half -32768, 65535
d: .byte -128
.text
main:
    lw s0, a
    lw s1, b
    lh s2, c
