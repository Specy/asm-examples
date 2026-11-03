# The text segment cannot be written
.text
main:
    la  t0, main
    sw  t0, 0(t0)
