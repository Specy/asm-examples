# Character literals and their escapes
.text
main:
    li s0, 'a'
    li s1, '\n'
    li s2, '\t'
    li s3, '\\'
    li s4, '\''
    li s5, '\0'
    li s6, '\101'
    li s7, ' '
    li s8, '"'
    li s9, '\"'
    li s10, '#'
    addi s11, s0, 'b'
    mv a0, s6
    li a7, 11
    ecall
