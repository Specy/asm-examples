# A data directive's list may go on over the next lines
.data
list: .word 1, 2,
      3, 4
      5
more: .byte 1
      2, 3
text: .asciz "a"
      "b"
.text
main:
    la s0, list
    lw s1, 16(s0)
    la s2, more
    la s3, text
