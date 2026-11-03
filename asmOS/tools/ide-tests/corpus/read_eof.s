# Input that runs out: an empty line for read string, then read integer refuses it
.data
buf: .space 8
.text
main:
    la  a0, buf
    li  a1, 8
    li  a7, 8
    ecall
    lw  s0, buf
    li  a7, 5
    ecall
