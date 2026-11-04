.data
buffer:     .space 34       # 31 binary digits, the terminator, and two spare bytes
buffer_end:                 # address immediately after those 34 reserved bytes
newline:    .asciz "\n"

.text
.globl main

# print_in_base(n, base): n in a0, base in a1
# Requires 0 <= n <= 2147483647 and 2 <= base <= 36.
print_in_base:
    la t0, buffer_end       # build the text backwards from the end
    addi t0, t0, -1         # move onto the final reserved byte
    sb zero, 0(t0)          # install the string terminator first
    li t2, 10               # split numeric digits from letter digits
digit:
    rem t1, a0, a1          # get the digit before a0 is overwritten
    div a0, a0, a1          # consume a0: it now holds the quotient
    blt t1, t2, number
    addi t1, t1, 55         # 10 + 55 is the character code for 'A'
    j store
number:
    addi t1, t1, 48         # 0 + 48 is the character code for '0'
store:
    addi t0, t0, -1         # move one byte in front of the existing text
    sb t1, 0(t0)
    bnez a0, digit          # continue while the quotient is not zero
    li a7, 4                # service 4 prints the string just built
    mv a0, t0
    ecall
    li a7, 4
    la a0, newline
    ecall
    ret

main:
    li a0, 48879
    li a1, 16
    jal print_in_base
    li a0, 48879
    li a1, 10
    jal print_in_base
    li a0, 48879
    li a1, 2
    jal print_in_base
    li a7, 10
    ecall
