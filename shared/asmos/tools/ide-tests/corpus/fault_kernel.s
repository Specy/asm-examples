# Nor is the kernel's half
.text
main:
    li  t0, 0x80000000
    sb  t0, 0(t0)
