# Branches and loops

The two ways a program leaves a straight line, one after the other. Build, then step through it and watch which instructions run.

First an if/else compares two numbers and keeps the larger. Only one of its two paths runs. Then a loop writes the numbers 1 to 10 into memory, one a pass: a store, a counter, and a jump back while there is more to do. Its result is in the memory panel.

| Target | Where the if/else leaves its answer                                 |
| ------ | ------------------------------------------------------------------- |
| M68K   | The larger of two signed numbers in `d2`, their distance in `d3`.   |
| MIPS   | The larger of two signed numbers in `$t2`, their distance in `$t4`. |
| RISC-V | The larger in `t2`, the distance in `t4`.                           |
| X86    | The larger of the signed qwords `x` and `y` in `r8`.                |
| Z80    | The larger of two unsigned bytes in `d`, their distance in `e`.     |

The editor's courses explain each half line by line. If and else: [M68K](https://asm-editor.specy.app/learn/courses/m68k/examples/if-else), [MIPS](https://asm-editor.specy.app/learn/courses/mips/examples/if-else), [RISC-V](https://asm-editor.specy.app/learn/courses/risc-v/examples/if-else), [X86](https://asm-editor.specy.app/learn/courses/x86/examples/if-else) and [Z80](https://asm-editor.specy.app/learn/courses/z80/examples/if-else). The counting loop: [M68K](https://asm-editor.specy.app/learn/courses/m68k/examples/counting-loop), [MIPS](https://asm-editor.specy.app/learn/courses/mips/examples/counting-loop), [RISC-V](https://asm-editor.specy.app/learn/courses/risc-v/examples/counting-loop), [X86](https://asm-editor.specy.app/learn/courses/x86/examples/counting-loop) and [Z80](https://asm-editor.specy.app/learn/courses/z80/examples/counting-loop).
