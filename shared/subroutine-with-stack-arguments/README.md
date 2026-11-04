# Subroutine with stack arguments

Calls `sum_of_squares`, which takes its arguments on the stack, calls `square` for each of them and returns the sum of the squares. It shows the whole calling convention: pushing arguments, building a frame for a local variable, saving the registers a subroutine borrows, and cleaning up after the call. Build, then step through it and watch the stack.

| Target | Where the answer is                                                                         |
| ------ | ------------------------------------------------------------------------------------------- |
| M68K   | `d7`: 3² + 4² = 25. The frame is made with `link` and `unlk`.                               |
| MIPS   | `$s1`. The fifth and sixth arguments go on the stack, after the four in `$a0` to `$a3`.     |
| RISC-V | `s2`: 25. Both arguments go on the stack, though RISC-V normally passes eight in registers. |
| X86    | `r12`. The seventh argument goes on the stack, after the six that travel in registers.      |
| Z80    | `hl`, with the frame built by hand around `ix`.                                             |

The editor's courses explain it line by line: [M68K](https://asm-editor.specy.app/learn/courses/m68k/examples/subroutine-with-stack-arguments), [MIPS](https://asm-editor.specy.app/learn/courses/mips/examples/subroutine-with-stack-arguments), [RISC-V](https://asm-editor.specy.app/learn/courses/risc-v/examples/subroutine-with-stack-arguments), [X86](https://asm-editor.specy.app/learn/courses/x86/examples/subroutine-with-stack-arguments) and [Z80](https://asm-editor.specy.app/learn/courses/z80/examples/subroutine-with-stack-arguments).
