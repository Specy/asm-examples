# read string stores UTF-8, its limit counted in characters and then in bytes
.data
buf: .space 32
.text
main:
    la  a0, buf
    li  a1, 5
    li  a7, 8
    ecall
    la  a0, buf
    li  a7, 4
    ecall
    la  t0, buf
    lw  s0, 0(t0)
    lw  s1, 4(t0)
