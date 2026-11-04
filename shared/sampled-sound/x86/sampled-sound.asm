default rel
%include "sys/syscall.inc"
%include "fcntl.inc"
%include "linux/soundcard.inc"

RATE    equ 22050                   ; samples a second
NOTE    equ RATE * 3 / 10           ; samples in one note: 0.3 seconds
LOUD    equ 12000                   ; the first sample's height; the most is 32767

section .rodata
dsp_path: db "/dev/dsp", 0
scale:    dd 262, 294, 330, 349, 392, 440, 494, 523, 0  ; Hz; 0 ends it

section .data
fragments: dd 0x0004000b            ; ask for 4 fragments of 2 to the 11th bytes
format:    dd AFMT_S16_LE           ; signed 16-bit samples, low byte first
channels:  dd 1                     ; mono
rate:      dd RATE

section .bss
wave:      resw NOTE                ; one note's samples

section .text
global _start
_start:
    mov eax, SYS_open
    lea rdi, [dsp_path]
    mov esi, O_WRONLY
    syscall
    test rax, rax
    js fail
    mov r12, rax                    ; r12: the sound device

    mov eax, SYS_ioctl              ; a small buffer: first, before anything else
    mov rdi, r12
    mov esi, SNDCTL_DSP_SETFRAGMENT
    lea rdx, [fragments]
    syscall
    mov eax, SYS_ioctl              ; then the format, the channels and the rate,
    mov rdi, r12
    mov esi, SNDCTL_DSP_SETFMT
    lea rdx, [format]
    syscall
    cmp dword [format], AFMT_S16_LE ; checking what each one wrote back
    jne fail
    mov eax, SYS_ioctl
    mov rdi, r12
    mov esi, SNDCTL_DSP_CHANNELS
    lea rdx, [channels]
    syscall
    cmp dword [channels], 1
    jne fail
    mov eax, SYS_ioctl
    mov rdi, r12
    mov esi, SNDCTL_DSP_SPEED
    lea rdx, [rate]
    syscall                         ; the rate it chose, near the one asked for

    lea rbx, [scale]                ; rbx: the next note
next_note:
    mov eax, [rbx]                  ; its frequency
    test eax, eax
    jz done
    shl eax, 16                     ; × 65536
    xor edx, edx
    div dword [rate]
    mov r14d, eax                   ; r14: how far through a cycle each sample goes
    xor r15d, r15d                  ; r15: how far it is, 65536 a whole cycle
    mov r13d, LOUD << 16            ; r13: the wave's height now, × 65536

    lea rdi, [wave]
    mov ecx, NOTE
fill:
    mov eax, r13d
    shr eax, 16                     ; the height: up for the first half of each cycle
    test r15d, 0x8000
    jz store
    neg eax                         ; down for the second half
store:
    mov [rdi], ax
    add rdi, 2
    add r15d, r14d                  ; on through the cycle
    mov eax, r13d
    shr eax, 12
    sub r13d, eax                   ; the height falls by 1/4096 a sample
    dec ecx
    jnz fill

    mov eax, SYS_write              ; waits while the buffer is full
    mov rdi, r12
    lea rsi, [wave]
    mov edx, NOTE * 2               ; two bytes a sample
    syscall
    add rbx, 4
    jmp next_note

done:
    mov eax, SYS_ioctl              ; wait until the last sample has played
    mov rdi, r12
    mov esi, SNDCTL_DSP_SYNC
    xor edx, edx
    syscall
    mov eax, SYS_exit
    xor edi, edi
    syscall
fail:
    mov eax, SYS_exit
    mov edi, 1
    syscall
