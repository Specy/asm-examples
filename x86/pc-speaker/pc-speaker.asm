default rel
%include "sys/syscall.inc"
%include "fcntl.inc"
%include "time.inc"
%include "linux/input.inc"

section .rodata
speaker_path: db "/dev/input/by-path/platform-pcspkr-event-spkr", 0
tune:                               ; Hz and ms for each note; 0 Hz is a rest, 0 ms the end
    dd 659, 150,  622, 150,  659, 150,  622, 150,  659, 150
    dd 494, 150,  587, 150,  523, 150,  440, 450,  0, 150
    dd 262, 150,  330, 150,  440, 150,  494, 450,  0, 150
    dd 330, 150,  415, 150,  494, 150,  523, 450
    dd 0, 0

section .data
tone: istruc input_event
        at input_event.type,  dw EV_SND
        at input_event.code,  dw SND_TONE
        at input_event.value, dd 0      ; the frequency in Hz, 0 for silence
      iend

section .bss
pause: resb timespec_size

section .text
global _start
_start:
    mov eax, SYS_open
    lea rdi, [speaker_path]
    mov esi, O_WRONLY
    syscall
    test rax, rax
    js fail
    mov r12, rax                    ; r12: the speaker
    lea rbx, [tune]                 ; rbx: the next note

next_note:
    mov eax, [rbx]                  ; its frequency
    mov ecx, [rbx + 4]              ; its duration in ms
    test ecx, ecx
    jz done                         ; 0 ms: the end of the tune
    mov [tone + input_event.value], eax
    mov eax, SYS_write
    mov rdi, r12
    lea rsi, [tone]
    mov edx, input_event_size
    syscall                         ; the tone starts, or stops for a rest

    mov eax, [rbx + 4]              ; wait for the note's duration:
    xor edx, edx
    mov ecx, 1000
    div ecx                         ; whole seconds in eax, ms left in edx
    mov [pause + timespec.tv_sec], rax
    imul rdx, rdx, 1000000          ; ms to ns
    mov [pause + timespec.tv_nsec], rdx
    mov eax, SYS_nanosleep
    lea rdi, [pause]
    xor esi, esi
    syscall

    add rbx, 8                      ; the next pair of dwords
    jmp next_note

done:
    mov dword [tone + input_event.value], 0
    mov eax, SYS_write
    mov rdi, r12
    lea rsi, [tone]
    mov edx, input_event_size
    syscall                         ; silence
    mov eax, SYS_exit
    xor edi, edi
    syscall
fail:
    mov eax, SYS_exit
    mov edi, 1
    syscall
