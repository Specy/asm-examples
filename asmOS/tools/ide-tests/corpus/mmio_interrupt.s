# Setting the interrupt-enable bit stops the program
.text
main:
    li t0, 0xffff0000
    li t1, 2
    sw t1, 0(t0)
