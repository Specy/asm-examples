# Several errors in one file, each on its own line, between good lines
.data
ok: .word 1
.text
main:
    li   t0, 1
    add  t0, t1
    li   t1, 2
    xyz
    addi t2, t2, 99999
    sw   t0, ok
    li   t3, 3
    lw   t4, ok+4
    j    main
