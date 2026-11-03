# @test limit=1003
# A program that never ends stops at the instruction limit
.text
main:
    li   s0, 0
loop:
    addi s0, s0, 1
    andi s1, s0, 7
    j    loop
