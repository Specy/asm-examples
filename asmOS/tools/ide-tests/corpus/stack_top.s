# The stack's highest word works; the bytes past it do not
.text
main:
    li  t0, 0x7ffffffc
    li  t1, 0x01020304
    sw  t1, 0(t0)
    lw  s0, 0(t0)
    lb  s1, 3(t0)          # 0x7fffffff: inside the word, but past the stack's base address
