count equ 6

    lea numbers, a0     ; address of the first word
    move.w #count-1, d1 ; 5 gives six passes with dbra
    clr.l d0            ; start the sum at zero
loop:
    add.w (a0)+, d0     ; add this word, then advance a0 by two bytes
    dbra d1, loop       ; repeat until all six words have been read
    simhalt             ; stop here: the sum is done

    org $2000
numbers: dc.w 4, 8, 15, 16, 23, 42
