# ebreak stops; the program then goes on
.text
main:
    li   s0, 1
    ebreak
    li   s1, 2
    ebreak
    li   s2, 3
