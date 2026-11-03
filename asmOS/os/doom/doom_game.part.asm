; DOOM's part game, os/doom/doom_game.c's Generated assembly, assembled here between labels
; that mark where its variables lie: the start and the end of its .data and of its .bss. DOOM
; keeps its world in variables that start as the compiler set them and never sets them back, so
; before a second game in the same asmOS, doom_fresh (os/doom/asmos_fresh.asm) puts back the
; values .data started with, which the first game kept in doom_game_initial, and clears .bss.
;
; Each part has a File like this one, and the Build assembles it in place of the Generated
; assembly it includes. The four cannot be one File: each Generated assembly names its labels
; L1, L2 and so on from the start, and NASM would find them defined four times. Nor can these
; labels be used from the part's own C, which declares what it uses `extern`: only from outside.

    global doom_game_data
    global doom_game_data_end
    global doom_game_bss
    global doom_game_bss_end
    global doom_game_initial

    section .data align=32
doom_game_data:
    section .bss align=32
doom_game_bss:

%include "doom_game.c.asm"

    section .data
doom_game_data_end:
    section .bss
doom_game_bss_end:
; outside the variables, so that a restore leaves it alone
doom_game_initial: resb doom_game_data_end - doom_game_data

