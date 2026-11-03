# Memory-mapped space past the four registers is plain memory
.text
main:
    li   t0, 0xffff0100
    li   t1, 0x12345678
    sw   t1, 0(t0)
    lw   s0, 0(t0)
    lh   s1, 2(t0)
    li   t2, 0xfffffffc
    sw   t1, 0(t2)
    lw   s2, 0(t2)
    lw   s3, -100          # an absolute address: 0xffffff9c, in the map
    li   t3, 0xffff0008
    lw   s4, 0(t3)
