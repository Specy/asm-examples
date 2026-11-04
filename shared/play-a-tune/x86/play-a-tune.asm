default rel
%include "sys/syscall.inc"
%include "fcntl.inc"
%include "time.inc"

section .rodata
midi_path: db "/dev/snd/midiC1D0", 0
piano:     db 0xc0, 0               ; program change: channel 1 plays instrument 0
tune:                               ; pitch, ms and drum for each note; pitch 0 ends it
    dd 64, 400, 36,  64, 400, 42,  65, 400, 38,  67, 400, 42
    dd 67, 400, 36,  65, 400, 42,  64, 400, 38,  62, 400, 42
    dd 60, 400, 36,  60, 400, 42,  62, 400, 38,  64, 400, 42
    dd 64, 600, 36,  62, 200, 42,  62, 800, 38
    dd 0

section .bss
message: resb 3
pause:   resb timespec_size

section .text
global _start

; send: the three-byte message dil, sil, dl to the MIDI port in r12.
; Changes rax, rcx, rdx, rsi, rdi and r11.
send:
    mov [message], dil
    mov [message + 1], sil
    mov [message + 2], dl
    mov eax, SYS_write
    mov rdi, r12
    lea rsi, [message]
    mov edx, 3
    syscall
    ret

_start:
    mov eax, SYS_open
    lea rdi, [midi_path]
    mov esi, O_WRONLY
    syscall
    test rax, rax
    js fail
    mov r12, rax                    ; r12: the MIDI port

    mov eax, SYS_write              ; choose the piano, two bytes
    mov rdi, r12
    lea rsi, [piano]
    mov edx, 2
    syscall

    lea rbx, [tune]                 ; rbx: the next note
next_note:
    mov r13d, [rbx]                 ; r13: its pitch
    test r13d, r13d
    jz done

    mov edi, 0x90                   ; note on, channel 1
    mov esi, r13d
    mov edx, 100                    ; how hard the key is struck, 1 to 127
    call send
    mov edi, 0x99                   ; note on, channel 10: a drum
    mov esi, [rbx + 8]              ; which drum
    mov edx, 80
    call send

    mov eax, [rbx + 4]              ; wait for the note's duration, under a second
    imul rax, rax, 1000000          ; ms to ns
    mov qword [pause + timespec.tv_sec], 0
    mov [pause + timespec.tv_nsec], rax
    mov eax, SYS_nanosleep
    lea rdi, [pause]
    xor esi, esi
    syscall

    mov edi, 0x80                   ; note off, channel 1
    mov esi, r13d
    xor edx, edx
    call send
    add rbx, 12                     ; the next three dwords
    jmp next_note

done:
    mov eax, SYS_close              ; waits until every byte has gone out
    mov rdi, r12
    syscall
    mov eax, SYS_exit
    xor edi, edi
    syscall
fail:
    mov eax, SYS_exit
    mov edi, 1
    syscall
