; doom_fresh: DOOM's variables as they were when asmOS started, before each game. DOOM keeps its
; world in variables that start as the compiler set them and never sets them back: a second game
; in the same asmOS would begin with what the first left in them. Each of its four parts is
; assembled between labels that mark where its variables lie (os/doom/doom_*.part.asm): the first
; game copies what each part's .data started with into the room after it, doom_<part>_initial,
; and every later one copies it back and clears the part's .bss. i_asmos.c calls it once no other
; DOOM is running, before DOOM itself begins.
;
; One rep movsb or rep stosb a range, which the emulator runs as one instruction, as os/blit.asm
; explains.
;
; void doom_fresh(void), with the System V calling convention: it changes only rax, rcx, rsi, rdi.

    default rel

    section .bss
initial_kept: resb 1                    ; the first game copied the values already

    section .text
    global doom_fresh

%macro part 1
    extern doom_%1_data, doom_%1_data_end, doom_%1_bss, doom_%1_bss_end, doom_%1_initial
%endmacro
    part game
    part play
    part render
    part world

; rcx = the bytes from label %1 to label %2
%macro length 2
    lea rcx, [%2]
    lea rax, [%1]
    sub rcx, rax
%endmacro

%macro keep 1
    lea rdi, [doom_%1_initial]
    lea rsi, [doom_%1_data]
    length doom_%1_data, doom_%1_data_end
    rep movsb
%endmacro

%macro restore 1
    lea rdi, [doom_%1_data]
    lea rsi, [doom_%1_initial]
    length doom_%1_data, doom_%1_data_end
    rep movsb
    lea rdi, [doom_%1_bss]
    length doom_%1_bss, doom_%1_bss_end
    xor eax, eax
    rep stosb
%endmacro

doom_fresh:
    cmp byte [initial_kept], 0
    jne .restore
    keep game
    keep play
    keep render
    keep world
    mov byte [initial_kept], 1
    ret
.restore:
    restore game
    restore play
    restore render
    restore world
    ret
