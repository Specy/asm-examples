# Exit with a code: the code and the pc after it
.text
main:
    li a0, 7
    li a7, 93
    ecall
    li a0, 99              # never runs
