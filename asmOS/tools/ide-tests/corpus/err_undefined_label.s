# Labels that are never defined
.text
main:
    j    nowhere
    la   t0, missing
    beq  t0, t1, absent
