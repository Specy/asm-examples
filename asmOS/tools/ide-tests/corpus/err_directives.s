# Directives used wrongly
.text
main:
    .word 5
    .space 4
    .asciz "x"
.data
    .space -1
    .align
    .align -2
    .word 1:0
.globl nothere
