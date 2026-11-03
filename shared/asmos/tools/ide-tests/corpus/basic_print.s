.data
x: .word 5
msg: .asciz "hello\n"
.text
main:
  li a0, 42
  li a7, 1
  ecall
  la t0, x
  lw a0, 0(t0)
  li t1, 0x80000000
  li a7, 34
  mv a0, t1
  ecall
  la a0, msg
  li a7, 4
  ecall
