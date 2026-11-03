# An instruction's name can be a label, as in RARS
.text
main:
    j    add
    li   s0, 1
add:
    li   s1, 2
    jal  lw
    li   s2, 3
    j    end
lw:
    li   s3, 4
    ret
end:
    nop
