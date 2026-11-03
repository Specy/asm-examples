# @test keys=xy
# Byte loads of the receiver's data register take the key as a word load does
.text
main:
    li  s0, 0xffff0000
    lbu s1, 4(s0)
    lw  s2, 0(s0)
    lw  s3, 4(s0)
    lw  s4, 0(s0)
    lw  s5, 4(s0)
    li  t0, 'A'
    sb  t0, 12(s0)
    sh  t0, 12(s0)
    lw  s6, 12(s0)
    li  t1, 1
    sw  t1, 0(s0)          # Ready is the device's to say
    lw  s7, 0(s0)
    sw  zero, 8(s0)
    lw  s8, 8(s0)
