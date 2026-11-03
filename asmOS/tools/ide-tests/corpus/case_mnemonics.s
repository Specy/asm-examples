# Mnemonics and directives in any case; registers only in lower case
.DATA
x: .WORD 7
.Text
main:
    LI   t0, 5
    Add  t1, t0, t0
    LW   t2, x
    ADDI t3, t2, 1
