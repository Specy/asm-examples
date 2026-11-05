# Subroutine with stack arguments

Computes 3² + 4² = 25 with `sum_of_squares`, which calls `square` for each value. X86 instead calls `sum7` to add 1 through 7, giving 28. Build, then step through the calls and watch the stack arguments, local variable and saved registers. The argument-passing convention differs by target.

| Target | Where the answer is                                                                         |
| ------ | ------------------------------------------------------------------------------------------- |
| M68K   | `d7`: 25. Both arguments go on the stack. The frame is made with `link` and `unlk`. |
| MIPS   | `$s1`: 25. The first four arguments go in `$a0` to `$a3` and are unused here. The fifth and sixth, 3 and 4, go on the stack. |
| RISC-V | `s2`: 25. Both arguments deliberately go on the stack for this example, though RISC-V normally passes the first eight integer arguments in registers. |
| X86    | `r12`: 28. The seventh argument goes on the stack, after the six that travel in registers. |
| Z80    | `hl`: 25. Both arguments go on the stack, with the frame built by hand around `ix`. |

The editor's courses explain it line by line: [M68K](https://asm-editor.specy.app/learn/courses/m68k/examples/subroutine-with-stack-arguments), [MIPS](https://asm-editor.specy.app/learn/courses/mips/examples/subroutine-with-stack-arguments), [RISC-V](https://asm-editor.specy.app/learn/courses/risc-v/examples/subroutine-with-stack-arguments), [X86](https://asm-editor.specy.app/learn/courses/x86/examples/subroutine-with-stack-arguments) and [Z80](https://asm-editor.specy.app/learn/courses/z80/examples/subroutine-with-stack-arguments).
