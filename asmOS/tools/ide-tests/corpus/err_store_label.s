# A store to a label needs a temporary register
.data
x: .word 1
.text
main:
    sw t0, x
