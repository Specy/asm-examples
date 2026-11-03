# A directive RARS does not know is ignored with a warning; the program runs
.text
.foo 3
.type main, @function
main:
    li a0, 3
    li a7, 1
    ecall
