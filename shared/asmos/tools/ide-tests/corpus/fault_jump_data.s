# A jump into the data segment cannot fetch an instruction
.data
x: .word 0x00000013
.text
main:
    la  t0, x
    jr  t0
