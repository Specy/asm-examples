# Text is UTF-8, in a string and through print string
.data
word: .asciz "héllo wörld é"
.text
main:
    la a0, word
    li a7, 4
    ecall
    la t0, word
    lbu s0, 1(t0)
    lbu s1, 2(t0)
