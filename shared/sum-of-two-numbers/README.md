# Sum of two numbers

Asks for two numbers and prints their sum. Build, then Run, and at each prompt type a whole number in the console and press Enter. The program waits until you do.

| Target | How it reads a number                                                           |
| ------ | ------------------------------------------------------------------------------- |
| M68K   | Task 18 of `trap #15` prints the prompt at `a1` and returns the number in `d1`. |
| MIPS   | Service 5 of `syscall` returns the number in `$v0`.                             |
| RISC-V | Service 5 of `ecall` returns the number in `a0`.                                |
| Z80    | `in a, (P_NUM)` reads it from the number port.                                  |

The editor's courses explain it line by line: [M68K](https://asm-editor.specy.app/learn/courses/m68k/examples/sum-of-two-numbers), [MIPS](https://asm-editor.specy.app/learn/courses/mips/examples/sum-of-two-numbers), [RISC-V](https://asm-editor.specy.app/learn/courses/risc-v/examples/sum-of-two-numbers) and [Z80](https://asm-editor.specy.app/learn/courses/z80/examples/sum-of-two-numbers).
