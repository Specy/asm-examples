# .eqv used wrongly
.eqv A
.eqv B, 1
.eqv B, 2
.eqv 5, 6
.text
main:
    li t0, B
