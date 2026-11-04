    lea text, a0        ; a0 points to the first character
    move.l a0, a1       ; begin the search at the same address
find_end:
    tst.b (a1)+         ; test a byte, then advance a1
    bne find_end
    subq.l #2, a1       ; a1 now points to the last character
swap_loop:
    cmp.l a0, a1        ; compare the right address with the left
    bls done            ; stop if a1 is at or before a0
    move.b (a0), d0     ; save the left byte
    move.b (a1), d1     ; save the right byte
    move.b d1, (a0)+    ; write right byte on left, then advance a0
    move.b d0, (a1)     ; write left byte on right
    subq.l #1, a1       ; move a1 one byte to the left
    bra swap_loop
done:

    org $2000
text: dc.b 'Assembly', 0
