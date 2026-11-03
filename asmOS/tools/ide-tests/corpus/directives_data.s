# The data directives: lists, repeats, truncation, strings, space and alignment
.data
a:  .byte 1, 2, -1, 255, 256      # 256 is cut to 0, with a warning
b:  .word 7                       # aligned to 4, and the label with it
c:  .half 1, -1
d:  .byte 9
e:  .half 0x12345                 # aligned to 2, cut to 0x2345
f:  .word 1:3                     # three words of 1
g:  .ascii "ab"
h:  .asciz "c\td"
i:  .string "\"q\"\\\n"
j:  .space 3
k:  .align 3
l:  .byte 5
    .align 0                      # automatic alignment off until the next .data
m:  .word 0x01020304              # not aligned now
n:  .half 0x0506
.data
o:  .word 8
p:  .byte 'x', '\n', '\\'
q:  .2byte 0x0102
r:  .4byte 0x03040506
.text
main:
    la s0, b
    la s1, e
    la s2, k
    la s3, l
    la s4, m
    la s5, n
    la s6, o
    la s7, r
    la s8, j
