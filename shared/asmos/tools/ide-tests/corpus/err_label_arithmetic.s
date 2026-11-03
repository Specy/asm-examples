# RARS takes no arithmetic on labels
.data
x: .word 1, 2
y: .word x+4
.text
main:
    la t0, x+4
