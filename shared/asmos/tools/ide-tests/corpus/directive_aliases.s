# The directives RARS takes for a C compiler's output: .bss, .zero, .p2align, .balign,
# .dword, .section and .global
.global main
.section .rodata
r: .word 0x11
.bss
z: .zero 6
.p2align 2
q: .word 0x22
.balign 8
d: .dword 0x33, -1
.8byte 7
.text
main:
    la s0, z
    la s1, q
    la s2, d
    lw s3, 0(s2)
    lw s4, 4(s2)
    lw s5, 8(s2)
    lw s6, 12(s2)
    lw s7, 16(s2)
