; DOOM's part world, os/doom/doom_world.c's Generated assembly, assembled here between labels
; that mark where its variables lie: the start and the end of its .data and of its .bss. DOOM
; keeps its world in variables that start as the compiler set them and never sets them back, so
; before a second game in the same asmOS, doom_fresh (os/doom/asmos_fresh.asm) puts back the
; values .data started with, which the first game kept in doom_world_initial, and clears .bss.
;
; Each part has a File like this one, and the Build assembles it in place of the Generated
; assembly it includes. The four cannot be one File: each Generated assembly names its labels
; L1, L2 and so on from the start, and NASM would find them defined four times. Nor can these
; labels be used from the part's own C, which declares what it uses `extern`: only from outside.

    global doom_world_data
    global doom_world_data_end
    global doom_world_bss
    global doom_world_bss_end
    global doom_world_initial

    section .data align=32
doom_world_data:
    section .bss align=32
doom_world_bss:

%include "doom_world.c.asm"

    section .data
doom_world_data_end:
    section .bss
doom_world_bss_end:
; outside the variables, so that a restore leaves it alone
doom_world_initial: resb doom_world_data_end - doom_world_data

