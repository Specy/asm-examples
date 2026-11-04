; @screen width=320 height=240
default rel
%include "sys/syscall.inc"
%include "fcntl.inc"
%include "sys/mman.inc"
%include "linux/fb.inc"
%include "linux/input.inc"

BOX        equ 20                   ; the square's width and height
SPEED      equ 3                    ; pixels a frame while an arrow is held
IDLE       equ 600                  ; frames without an EV_KEY event before exit
BACKGROUND equ 0x00101820
BOX_COLOUR equ 0x0000c060

section .rodata
fb_path:  db "/dev/fb0", 0
kbd_path: db "/dev/input/by-path/platform-i8042-serio-0-event-kbd", 0

section .data
x:        dd 150                    ; the square's top-left corner
y:        dd 110
page:     dd 1                      ; the page being drawn, the one not shown
flip:     dd 1                      ; 1 to change pages each frame, 0 with only one
drawn_x:  dd 0, 0                   ; where each page last had the square:
drawn_y:  dd 0, 0                   ; nowhere yet, and erasing there changes nothing
left:     db 0                      ; 1 while that arrow is held
right:    db 0
up:       db 0
down:     db 0

section .bss
var:      resb fb_var_screeninfo_size
fix:      resb fb_fix_screeninfo_size
pixels:   resq 1
event:    resb input_event_size

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
    mov r12, rax                    ; r12: the framebuffer
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
    mov esi, [fix + fb_fix_screeninfo.smem_len]
    mov edx, PROT_READ | PROT_WRITE
    mov r10d, MAP_SHARED
    mov r8, r12
    xor r9d, r9d
    syscall
    cmp rax, -4095
    jae fail
    mov [pixels], rax
    mov r13d, [fix + fb_fix_screeninfo.line_length]
    mov rbx, rax                    ; paint every page
    xor edi, edi
    xor esi, esi
    mov edx, [var + fb_var_screeninfo.xres]
    mov ecx, [var + fb_var_screeninfo.yres_virtual]
    mov r8d, BACKGROUND
    call fill_rect

    mov eax, SYS_open
    lea rdi, [kbd_path]
    mov esi, O_RDONLY | O_NONBLOCK  ; a read with no event returns at once
    syscall
    test rax, rax
    js fail
    mov r14, rax                    ; r14: the keyboard
    mov r15d, IDLE                  ; r15: frames left without an EV_KEY event

frame:
read_event:                         ; take every event waiting, one at a time
    mov eax, SYS_read
    mov rdi, r14
    lea rsi, [event]
    mov edx, input_event_size
    syscall
    cmp rax, input_event_size
    jne events_done                 ; stop reading on any result other than a full event
    cmp word [event + input_event.type], EV_KEY
    jne read_event                  ; skip MSC_SCAN, SYN_REPORT and the rest
    mov r15d, IDLE                  ; any key event restarts the frame countdown
    mov eax, [event + input_event.value]
    cmp eax, 2
    je read_event                   ; a repeat: the key is still held
    movzx ecx, word [event + input_event.code]
    cmp ecx, KEY_ESC
    je done
    cmp ecx, KEY_LEFT
    jne not_left
    mov [left], al                  ; 1 when it goes down, 0 when it comes up
not_left:
    cmp ecx, KEY_RIGHT
    jne not_right
    mov [right], al
not_right:
    cmp ecx, KEY_UP
    jne not_up
    mov [up], al
not_up:
    cmp ecx, KEY_DOWN
    jne read_event
    mov [down], al
    jmp read_event
events_done:

    movzx eax, byte [right]         ; across: right - left is 1, -1 or 0
    movzx ecx, byte [left]
    sub eax, ecx
    imul eax, SPEED
    add eax, [x]
    jns x_low                       ; past the left edge: stop there
    xor eax, eax
x_low:
    mov ecx, [var + fb_var_screeninfo.xres]
    sub ecx, BOX
    cmp eax, ecx
    jle x_high                      ; past the right edge: stop there
    mov eax, ecx
x_high:
    mov [x], eax

    movzx eax, byte [down]          ; down the Screen: down - up
    movzx ecx, byte [up]
    sub eax, ecx
    imul eax, SPEED
    add eax, [y]
    jns y_low
    xor eax, eax
y_low:
    mov ecx, [var + fb_var_screeninfo.yres]
    sub ecx, BOX
    cmp eax, ecx
    jle y_high
    mov eax, ecx
y_high:
    mov [y], eax

    mov eax, [page]                 ; where this page has the square
    lea rcx, [drawn_x]
    mov edi, [rcx + rax*4]
    lea rcx, [drawn_y]
    mov esi, [rcx + rax*4]
    cmp edi, [x]
    jne redraw
    cmp esi, [y]
    je present                      ; already where it is now: nothing to draw
redraw:
    mov eax, [page]                 ; rbx: the page to draw
    imul eax, [var + fb_var_screeninfo.yres]
    imul rax, r13
    add rax, [pixels]
    mov rbx, rax
    mov edx, BOX                    ; erase the square this page had
    mov ecx, BOX
    mov r8d, BACKGROUND
    call fill_rect
    mov eax, [page]                 ; and draw it where it is now
    lea rcx, [drawn_x]
    mov edi, [x]
    mov [rcx + rax*4], edi
    lea rcx, [drawn_y]
    mov esi, [y]
    mov [rcx + rax*4], esi
    mov edx, BOX
    mov ecx, BOX
    mov r8d, BOX_COLOUR
    call fill_rect

present:
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
done:
    mov eax, SYS_exit
    xor edi, edi
    syscall
fail:
    mov eax, SYS_exit
    mov edi, 1
    syscall
