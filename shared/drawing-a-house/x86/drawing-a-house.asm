; @screen width=320 height=240
default rel
%include "sys/syscall.inc"
%include "fcntl.inc"
%include "sys/mman.inc"
%include "linux/fb.inc"

HORIZON equ 180                     ; the first line of grass
SUN_X   equ 260                     ; the sun's centre
SUN_Y   equ 50
SUN_R   equ 24                      ; and its radius

SKY     equ 0x0070b0e0              ; colours are 0x00RRGGBB
GRASS   equ 0x003c9648
SUN     equ 0x00ffd200
WALL    equ 0x00c07040
ROOF    equ 0x00a02020
DOOR    equ 0x00704020
WINDOW  equ 0x00fff0a0

section .rodata
fb_path: db "/dev/fb0", 0

section .bss
var:    resb fb_var_screeninfo_size
fix:    resb fb_fix_screeninfo_size

section .text
global _start

; fill_rect: fill edx columns and ecx rows with the colour in r8d.
; edi and esi give the top-left column and row. Non-positive width or height
; draws nothing. The rectangle must fit on the Screen.
; Reads rbx (the top-left pixel) and r13 (bytes a line); changes rax, rcx, rsi,
; rdi, r9 and r10.
fill_rect:
    test edx, edx
    jle .done
    test ecx, ecx
    jle .done
    mov r9d, ecx                    ; r9: lines left to fill
    imul rsi, r13                   ; y × line_length
    lea r10, [rbx + rsi]
    lea r10, [r10 + rdi*4]          ; r10: the rectangle's top-left pixel
    mov eax, r8d                    ; the colour rep stosd stores
.line:
    mov rdi, r10
    mov ecx, edx
    rep stosd                       ; ecx pixels of eax, from rdi onward
    add r10, r13                    ; the same column, one line down
    dec r9d
    jnz .line
.done:
    ret

_start:
    mov eax, SYS_open
    lea rdi, [fb_path]
    mov esi, O_RDWR
    syscall
    test rax, rax
    js fail                         ; a negative result is an error
    mov r12, rax                    ; r12: the framebuffer's descriptor

    mov eax, SYS_ioctl
    mov rdi, r12
    mov esi, FBIOGET_VSCREENINFO
    lea rdx, [var]
    syscall                         ; xres and yres: the size in pixels
    mov eax, SYS_ioctl
    mov rdi, r12
    mov esi, FBIOGET_FSCREENINFO
    lea rdx, [fix]
    syscall                         ; smem_len and line_length

    mov eax, SYS_mmap
    xor edi, edi                    ; at any address
    mov esi, [fix + fb_fix_screeninfo.smem_len]
    mov edx, PROT_READ | PROT_WRITE
    mov r10d, MAP_SHARED            ; stores reach the Screen
    mov r8, r12
    xor r9d, r9d
    syscall
    cmp rax, -4095
    jae fail
    mov rbx, rax                    ; rbx: the top-left pixel
    mov r13d, [fix + fb_fix_screeninfo.line_length]

    xor edi, edi                    ; the sky, across the whole width
    xor esi, esi
    mov edx, [var + fb_var_screeninfo.xres]
    mov ecx, HORIZON
    mov r8d, SKY
    call fill_rect

    xor edi, edi                    ; the grass, from the horizon down
    mov esi, HORIZON
    mov edx, [var + fb_var_screeninfo.xres]
    mov ecx, [var + fb_var_screeninfo.yres]
    sub ecx, HORIZON
    mov r8d, GRASS
    call fill_rect

; the sun: a disc is a stack of lines, the widest across its middle
    mov r14d, -(SUN_R - 1)          ; r14: the line, counted from the centre
sun_line:
    mov r15d, SUN_R                 ; r15: half the line's width
sun_fit:
    dec r15d                        ; one pixel narrower
    mov eax, r15d
    imul eax, eax                   ; half width²
    mov ecx, r14d
    imul ecx, ecx                   ; + line²
    add eax, ecx
    cmp eax, SUN_R * SUN_R
    jge sun_fit                     ; outside the circle: narrower still
    mov edi, SUN_X
    sub edi, r15d                   ; left edge: centre minus half width
    lea esi, [r14 + SUN_Y]
    lea edx, [r15*2 + 1]            ; width: both halves and the centre pixel
    mov ecx, 1
    mov r8d, SUN
    call fill_rect
    inc r14d
    cmp r14d, SUN_R
    jl sun_line

    mov edi, 90                     ; the wall
    mov esi, 110
    mov edx, 120
    mov ecx, 80
    mov r8d, WALL
    call fill_rect

; the roof: 30 rows; each end moves inward 2 pixels on the next row
    xor r14d, r14d                  ; r14: the roof's line, 0 at the bottom
roof_line:
    lea edi, [r14*2 + 80]           ; starts 2 pixels further right a line
    mov esi, 109
    sub esi, r14d                   ; one line higher
    mov edx, 140
    lea eax, [r14*4]
    sub edx, eax                    ; 4 pixels shorter
    mov ecx, 1
    mov r8d, ROOF
    call fill_rect
    inc r14d
    cmp r14d, 30
    jb roof_line

    mov edi, 135                    ; the door
    mov esi, 145
    mov edx, 30
    mov ecx, 45
    mov r8d, DOOR
    call fill_rect
    mov edi, 105                    ; two windows
    mov esi, 125
    mov edx, 20
    mov ecx, 20
    mov r8d, WINDOW
    call fill_rect
    mov edi, 175
    mov esi, 125
    mov edx, 20
    mov ecx, 20
    mov r8d, WINDOW
    call fill_rect

    mov eax, SYS_exit
    xor edi, edi
    syscall
fail:
    mov eax, SYS_exit
    mov edi, 1
    syscall
