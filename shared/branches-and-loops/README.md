# Branches and loops

This program shows the two basic kinds of control flow: an if/else and a loop. Step through it and watch which instructions run.

First, an if/else compares two numbers and keeps the larger one. Only one of its two branches runs. Then a loop writes the numbers 1 to 10 into an array, one number per iteration. Look at `numbers` in the memory panel to see the array.

| Target | Where the if/else puts its result |
| ------ | --------------------------------- |
| M68K   | The larger of two signed numbers in `d2`, and the distance between them in `d3`. |
| MIPS   | The larger of two signed numbers in `$t2`, and the distance between them in `$t4`. |
| RISC-V | The larger number in `t2`, and the distance between them in `t4`. |
| X86    | The larger of the signed qwords `x` and `y` in `r8`. |
| Z80    | The larger of two unsigned bytes in `d`, and the distance between them in `e`. |

Go to the course's lectures to read a line-by-line explanation.

- The if/else: [M68K](https://asm-editor.specy.app/learn/courses/m68k/examples/if-else), [MIPS](https://asm-editor.specy.app/learn/courses/mips/examples/if-else), [RISC-V](https://asm-editor.specy.app/learn/courses/risc-v/examples/if-else), [X86](https://asm-editor.specy.app/learn/courses/x86/examples/if-else) or [Z80](https://asm-editor.specy.app/learn/courses/z80/examples/if-else).
- The loop: [M68K](https://asm-editor.specy.app/learn/courses/m68k/examples/counting-loop), [MIPS](https://asm-editor.specy.app/learn/courses/mips/examples/counting-loop), [RISC-V](https://asm-editor.specy.app/learn/courses/risc-v/examples/counting-loop), [X86](https://asm-editor.specy.app/learn/courses/x86/examples/counting-loop) or [Z80](https://asm-editor.specy.app/learn/courses/z80/examples/counting-loop).
