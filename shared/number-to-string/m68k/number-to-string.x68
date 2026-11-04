    move.l #48879, d0       ; number to convert
    move.l #16, d1          ; base 16
    bsr print_in_base
    move.l #48879, d0
    move.l #10, d1          ; base 10
    bsr print_in_base
    move.l #48879, d0
    move.l #2, d1           ; base 2
    bsr print_in_base
    move.b #9, d0           ; end the program
    trap #15

; print_in_base: d0 = number (0..131071), d1 = base (2..36)
print_in_base:
    lea buffer_end, a1      ; start just beyond the buffer
    clr.b -(a1)             ; put the zero terminator at the end
digit:
    move.l d0, d2
    divu d1, d2             ; high word = remainder, low word = quotient
    move.l d2, d3
    swap d3
    andi.l #$FFFF, d3       ; d3 = remainder: the next digit
    andi.l #$FFFF, d2       ; d2 = quotient: the number still to convert
    cmp.b #9, d3
    bhi letter
    add.b #'0', d3          ; 0..9 become '0'..'9'
    bra store
letter:
    add.b #'A'-10, d3       ; 10..35 become 'A'..'Z'
store:
    move.b d3, -(a1)        ; put this digit before the earlier ones
    move.l d2, d0           ; use the quotient on the next pass
    bne digit
    move.b #13, d0          ; print the string at a1 and a new line
    trap #15
    rts

    org $2000
buffer:     ds.b 18         ; at most 17 binary digits plus the terminator
buffer_end:
