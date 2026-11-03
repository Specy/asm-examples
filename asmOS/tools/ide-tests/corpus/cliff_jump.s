# A jump to the text segment past the program ends it, as running off the end does
.text
main:
    li  s0, 1
    li  t0, 0x00500000
    jr  t0
    li  s0, 2
