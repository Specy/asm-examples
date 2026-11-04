count equ 10

* If and else: the larger of a and b in d2, their distance in d3
    move.l #37, d0      ; a = 37
    move.l #64, d1      ; b = 64

    cmp.l d1, d0        ; set flags from a - b; keep a and b
    bge a_is_bigger     ; if a >= b, use a
    move.l d1, d2       ; otherwise bigger = b
    bra done            ; skip the other assignment
a_is_bigger:
    move.l d0, d2       ; bigger = a
done:

    move.l d0, d3       ; start with a
    sub.l d1, d3        ; d3 = a - b
    bpl positive        ; if d3 is non-negative, keep it
    neg.l d3            ; otherwise make it positive
positive:

* A loop: the numbers 1 to 10 into the words at numbers
    lea numbers, a0     ; address of the first word
    move.w #count-1, d5 ; 9 gives ten passes with dbra
    move.w #1, d4       ; first value to write
fill:
    move.w d4, (a0)     ; write d4 to memory at the address in a0
    addq.l #2, a0       ; advance to the next word
    addq.w #1, d4       ; prepare the next value
    dbra d5, fill

    org $2000
numbers: ds.w count
