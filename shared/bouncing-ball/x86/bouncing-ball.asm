; @screen width=320 height=240
default rel
%include "sys/syscall.inc"
%include "fcntl.inc"
%include "sys/mman.inc"
%include "linux/fb.inc"

BALL        equ 16                  ; the ball's width and height
FRAMES      equ 600                 ; at least ten seconds, at up to 60 frames a second
BACKGROUND  equ 0x00101820
BALL_COLOUR equ 0x00ffcc33

section .rodata
fb_path:   db "/dev/fb0", 0
ball_rows: db 6, 10, 12, 14, 14, 16, 16, 16, 16, 16, 16, 14, 14, 12, 10, 6

section .data
x:         dd 40                    ; the ball's top-left corner
y:         dd 30
step_x:    dd 3                     ; pixels a frame, + right or down, - left or up
step_y:    dd 2
page:      dd 1                     ; the page being drawn, the one not shown
flip:      dd 1                     ; 1 to change pages each frame, 0 with only one
drawn_x:   dd 40, 40                ; where each page last had the ball
drawn_y:   dd 30, 30

section .bss
var:       resb fb_var_screeninfo_size
fix:       resb fb_fix_screeninfo_size
pixels:    resq 1                   ; the first page's top-left pixel

section .text
global _start

; fill_rect: a rectangle of colour r8d, edx pixels wide and ecx lines high, whose
; top-left corner is at column edi of line esi of the page rbx points to.
; Reads rbx and r13 (bytes a line); changes rax, rcx, rsi, rdi, r9 and r10.
fill_rect:
    test edx, edx
    jle .done
    test ecx, ecx
    jle .done
    mov r9d, ecx
    imul rsi, r13
    lea r10, [rbx + rsi]
    lea r10, [r10 + rdi*4]
    mov eax, r8d
.line:
    mov rdi, r10
    mov ecx, edx
    rep stosd
    add r10, r13
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
    js fail
    mov r12, rax                    ; r12: the framebuffer's descriptor
    mov eax, SYS_ioctl
    mov rdi, r12
    mov esi, FBIOGET_VSCREENINFO
    lea rdx, [var]
    syscall
    mov eax, [var + fb_var_screeninfo.yres]   ; is there room for a second page?
    add eax, eax
    cmp [var + fb_var_screeninfo.yres_virtual], eax
    jae two_pages
    mov dword [page], 0             ; no: draw every frame on the page shown
    mov dword [flip], 0
two_pages:
    mov eax, SYS_ioctl
    mov rdi, r12
    mov esi, FBIOGET_FSCREENINFO
    lea rdx, [fix]
    syscall
    mov eax, SYS_mmap
    xor edi, edi
    mov esi, [fix + fb_fix_screeninfo.smem_len]   ; every page
    mov edx, PROT_READ | PROT_WRITE
    mov r10d, MAP_SHARED
    mov r8, r12
    xor r9d, r9d
    syscall
    cmp rax, -4095
    jae fail
    mov [pixels], rax
    mov r13d, [fix + fb_fix_screeninfo.line_length]

    mov rbx, rax                    ; paint every page, yres_virtual lines
    xor edi, edi
    xor esi, esi
    mov edx, [var + fb_var_screeninfo.xres]
    mov ecx, [var + fb_var_screeninfo.yres_virtual]
    mov r8d, BACKGROUND
    call fill_rect

    mov r15d, FRAMES                ; r15: frames left
frame:
    mov eax, [page]                 ; rbx: the top-left pixel of the page to draw
    imul eax, [var + fb_var_screeninfo.yres]
    imul rax, r13
    add rax, [pixels]
    mov rbx, rax

    mov eax, [page]                 ; erase the rectangle at this page's saved ball position
    lea rcx, [drawn_x]
    mov edi, [rcx + rax*4]
    lea rcx, [drawn_y]
    mov esi, [rcx + rax*4]
    mov edx, BALL
    mov ecx, BALL
    mov r8d, BACKGROUND
    call fill_rect

    mov eax, [x]                    ; move across, and turn around at an edge
    add eax, [step_x]
    js turn_x                       ; past the left edge
    mov ecx, [var + fb_var_screeninfo.xres]
    sub ecx, BALL
    cmp eax, ecx
    jle x_done                      ; not past the right edge
turn_x:
    neg dword [step_x]
    add eax, [step_x]               ; back where it was
    add eax, [step_x]               ; and one step the other way
x_done:
    mov [x], eax

    mov eax, [y]                    ; the same vertically
    add eax, [step_y]
    js turn_y
    mov ecx, [var + fb_var_screeninfo.yres]
    sub ecx, BALL
    cmp eax, ecx
    jle y_done
turn_y:
    neg dword [step_y]
    add eax, [step_y]
    add eax, [step_y]
y_done:
    mov [y], eax

    mov eax, [page]                 ; this page will have the ball here
    lea rcx, [drawn_x]
    mov edx, [x]
    mov [rcx + rax*4], edx
    lea rcx, [drawn_y]
    mov edx, [y]
    mov [rcx + rax*4], edx

    xor r14d, r14d                  ; draw the ball, one row at a time
ball_row:
    lea rax, [ball_rows]
    movzx edx, byte [rax + r14]     ; this row's width
    mov edi, BALL
    sub edi, edx
    shr edi, 1                      ; centred: (BALL - width) / 2 from the left
    add edi, [x]
    mov esi, [y]
    add esi, r14d
    mov ecx, 1
    mov r8d, BALL_COLOUR
    call fill_rect
    inc r14d
    cmp r14d, BALL
    jb ball_row

    mov eax, [page]                 ; show the page just drawn, at the vertical blank
    imul eax, [var + fb_var_screeninfo.yres]
    mov [var + fb_var_screeninfo.yoffset], eax
    mov eax, SYS_ioctl
    mov rdi, r12
    mov esi, FBIOPAN_DISPLAY
    lea rdx, [var]
    syscall

    mov eax, [flip]                 ; the next frame goes on the other page
    xor [page], eax
    dec r15d
    jnz frame

    mov eax, SYS_exit
    xor edi, edi
    syscall
fail:
    mov eax, SYS_exit
    mov edi, 1
    syscall
