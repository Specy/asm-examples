; @screen width=320 height=240
default rel
%include "sys/syscall.inc"
%include "fcntl.inc"
%include "sys/mman.inc"
%include "poll.inc"
%include "linux/fb.inc"
%include "linux/input.inc"

BRUSH   equ 6                       ; the brush's width and height
QUIET   equ 30000                   ; ms without an event before the end
PAPER   equ 0x00f4f0e6

section .rodata
fb_path: db "/dev/fb0", 0
colours: dd 0x00202020, 0x00e04040, 0x00f0a020, 0x0040b040, 0x002080e0, 0x008040c0
COLOURS  equ ($ - colours) / 4      ; how many there are: 6

section .data
event_path: db "/dev/input/event0", 0
colour:  dd 1                       ; the colour the wheel chose, 0 to COLOURS - 1
x:       dd 0                       ; the pointer, in the tablet's units
y:       dd 0
pressed: db 0                       ; 1 while the left button is down

section .bss
var:     resb fb_var_screeninfo_size
fix:     resb fb_fix_screeninfo_size
types:   resb 4                     ; one bit for each type of event a device sends
xinfo:   resb input_absinfo_size    ; the range of each axis
yinfo:   resb input_absinfo_size
event:   resb input_event_size
waiter:  resb pollfd_size

section .text
global _start

; fill_rect: a rectangle of colour r8d, edx pixels wide and ecx lines high, whose
; top-left corner is at column edi of line esi.
; Reads rbx (the top-left pixel) and r13 (bytes a line); changes rax, rcx, rsi,
; rdi, r9 and r10.
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

; to_pixel: the pixel of the position eax on an axis of edx pixels whose range
; ends at ecx: (eax × edx + ecx - 1) ÷ ecx, into eax. Changes edx.
to_pixel:
    imul eax, edx
    lea eax, [rax + rcx - 1]
    xor edx, edx
    div ecx
    ret

; paint: a BRUSH by BRUSH square of the chosen colour around the pointer, kept on
; the Screen. Changes the registers fill_rect changes, and rdx and r8.
paint:
    mov eax, [x]
    mov edx, [var + fb_var_screeninfo.xres]
    mov ecx, [xinfo + input_absinfo.maximum]
    call to_pixel
    sub eax, BRUSH / 2              ; the brush's left edge
    jns .left_inside
    xor eax, eax
.left_inside:
    mov ecx, [var + fb_var_screeninfo.xres]
    sub ecx, BRUSH
    cmp eax, ecx
    jle .right_inside
    mov eax, ecx
.right_inside:
    mov edi, eax

    mov eax, [y]                    ; the same down the Screen
    mov edx, [var + fb_var_screeninfo.yres]
    mov ecx, [yinfo + input_absinfo.maximum]
    call to_pixel
    sub eax, BRUSH / 2
    jns .top_inside
    xor eax, eax
.top_inside:
    mov ecx, [var + fb_var_screeninfo.yres]
    sub ecx, BRUSH
    cmp eax, ecx
    jle .bottom_inside
    mov eax, ecx
.bottom_inside:
    mov esi, eax

    mov eax, [colour]
    lea r8, [colours]
    mov r8d, [r8 + rax*4]
    mov edx, BRUSH
    mov ecx, BRUSH
    jmp fill_rect                   ; its ret returns from paint

; show_colour: the chosen colour in a framed square at the top-left corner.
show_colour:
    mov edi, 2
    mov esi, 2
    mov edx, 20
    mov ecx, 20
    xor r8d, r8d                    ; the frame, black
    call fill_rect
    mov eax, [colour]
    lea r8, [colours]
    mov r8d, [r8 + rax*4]
    mov edi, 4
    mov esi, 4
    mov edx, 16
    mov ecx, 16
    jmp fill_rect

_start:
    mov eax, SYS_open
    lea rdi, [fb_path]
    mov esi, O_RDWR
    syscall
    test rax, rax
    js fail
    mov r12, rax
    mov eax, SYS_ioctl
    mov rdi, r12
    mov esi, FBIOGET_VSCREENINFO
    lea rdx, [var]
    syscall
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
    mov rbx, rax                    ; rbx: the top-left pixel
    mov r13d, [fix + fb_fix_screeninfo.line_length]

    xor edi, edi                    ; a clean sheet of paper
    xor esi, esi
    mov edx, [var + fb_var_screeninfo.xres]
    mov ecx, [var + fb_var_screeninfo.yres]
    mov r8d, PAPER
    call fill_rect
    call show_colour

; find the tablet: the first of event0 to event9 that reports a position
    xor r14d, r14d                  ; r14: the number to try
find_tablet:
    lea eax, [r14 + '0']
    mov [event_path + 16], al       ; the digit of /dev/input/event0
    mov eax, SYS_open
    lea rdi, [event_path]
    mov esi, O_RDONLY | O_NONBLOCK
    syscall
    test rax, rax
    js next_device                  ; no such device
    mov r12, rax
    mov eax, SYS_ioctl
    mov rdi, r12
    mov esi, EVIOCGBIT(0, 4)        ; which types of event it sends
    lea rdx, [types]
    syscall
    test dword [types], 1 << EV_ABS
    jnz found_tablet                ; positions: this is the tablet
    mov eax, SYS_close
    mov rdi, r12
    syscall
next_device:
    inc r14d
    cmp r14d, 10
    jb find_tablet
    jmp fail
found_tablet:                       ; r12: the tablet
    mov eax, SYS_ioctl
    mov rdi, r12
    mov esi, EVIOCGABS(ABS_X)       ; x's range, and where the pointer is now
    lea rdx, [xinfo]
    syscall
    mov eax, SYS_ioctl
    mov rdi, r12
    mov esi, EVIOCGABS(ABS_Y)
    lea rdx, [yinfo]
    syscall
    mov eax, [xinfo + input_absinfo.value]
    mov [x], eax
    mov eax, [yinfo + input_absinfo.value]
    mov [y], eax

wait_for_events:
    mov [waiter + pollfd.fd], r12d
    mov word [waiter + pollfd.events], POLLIN
    mov eax, SYS_poll
    lea rdi, [waiter]
    mov esi, 1                      ; one descriptor
    mov edx, QUIET                  ; for at most QUIET ms
    syscall
    test rax, rax
    jle done                        ; 0: nothing for QUIET ms
read_event:
    mov eax, SYS_read
    mov rdi, r12
    lea rsi, [event]
    mov edx, input_event_size
    syscall
    cmp rax, input_event_size
    jne wait_for_events             ; -EAGAIN: all read
    movzx eax, word [event + input_event.type]
    movzx ecx, word [event + input_event.code]
    mov edx, [event + input_event.value]

    cmp eax, EV_ABS                 ; a move: where the pointer is now
    jne not_move
    cmp ecx, ABS_X
    jne not_x
    mov [x], edx
not_x:
    cmp ecx, ABS_Y
    jne read_event
    mov [y], edx
    jmp read_event
not_move:
    cmp eax, EV_KEY                 ; a button: 1 down, 0 up
    jne not_button
    cmp ecx, BTN_LEFT
    jne read_event
    mov [pressed], dl
    jmp read_event
not_button:
    cmp eax, EV_REL                 ; the wheel: 1 a notch away from you, -1 toward
    jne not_wheel
    cmp ecx, REL_WHEEL
    jne read_event                  ; skip REL_WHEEL_HI_RES, the same turn finer
    add edx, [colour]
    jns not_below
    add edx, COLOURS                ; before the first colour: the last
not_below:
    cmp edx, COLOURS
    jb not_above
    sub edx, COLOURS                ; after the last: the first
not_above:
    mov [colour], edx
    call show_colour
    jmp read_event
not_wheel:
    cmp eax, EV_SYN                 ; the end of a report: paint if the button is down
    jne read_event
    cmp ecx, SYN_REPORT
    jne read_event
    cmp byte [pressed], 0
    je read_event
    call paint
    jmp read_event

done:
    mov eax, SYS_exit
    xor edi, edi
    syscall
fail:
    mov eax, SYS_exit
    mov edi, 1
    syscall
