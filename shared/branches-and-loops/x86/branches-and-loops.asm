default rel
global _start

section .data
x:      dq 7
y:      dq 12

section .bss
numbers: resq 10            ; ten eight-byte slots

section .text
_start:
; If and else: the larger of x and y in r8
    mov rax, [x]            ; start with x as the answer
    cmp rax, [y]            ; set flags for x - y; leave rax unchanged
    jge .done               ; signed x >= y: keep x
    mov rax, [y]            ; otherwise choose y
.done:
    mov r8, rax             ; keep the answer for inspection

; A loop: the numbers 1 to 10 into the slots at numbers
    xor rcx, rcx            ; index = 0
.fill:
    mov rax, rcx
    inc rax                 ; value = index + 1
    mov [numbers + rcx*8], rax
    inc rcx                 ; next index
    cmp rcx, 10
    jb .fill                ; repeat while unsigned rcx < 10

    mov rax, 60             ; syscall 60: exit
    xor rdi, rdi
    syscall
