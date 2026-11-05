# Subroutine with stack arguments

This program calls a subroutine that takes its arguments on the stack and keeps a local variable in a stack frame. Step through the calls and watch the stack: the arguments, the local variable and the saved registers.

On M68K, MIPS, RISC-V and Z80, `sum_of_squares` computes 3² + 4² = 25, calling `square` for each value. On X86, `sum7` adds the numbers 1 to 7, giving 28: x86 passes the first six arguments in registers, so a subroutine needs seven arguments before one goes on the stack.

Each target has its own calling convention:

| Target | Result | How the arguments are passed |
| ------ | ------ | ---------------------------- |
| M68K   | 25 in `d7` | Both arguments go on the stack. `link` and `unlk` make and remove the frame. |
| MIPS   | 25 in `$s1` | The first four arguments go in `$a0` to `$a3`, unused here. The fifth and sixth, 3 and 4, go on the stack. |
| RISC-V | 25 in `s2` | Both arguments go on the stack, for this example. RISC-V normally passes the first eight integer arguments in registers. |
| X86    | 28 in `r12` | The first six arguments go in registers and the seventh on the stack. |
| Z80    | 25 in `hl` | Both arguments go on the stack, and the frame is built by hand around `ix`. |

Go to the course's lecture to read a line-by-line explanation: [M68K](https://asm-editor.specy.app/learn/courses/m68k/examples/subroutine-with-stack-arguments), [MIPS](https://asm-editor.specy.app/learn/courses/mips/examples/subroutine-with-stack-arguments), [RISC-V](https://asm-editor.specy.app/learn/courses/risc-v/examples/subroutine-with-stack-arguments), [X86](https://asm-editor.specy.app/learn/courses/x86/examples/subroutine-with-stack-arguments) or [Z80](https://asm-editor.specy.app/learn/courses/z80/examples/subroutine-with-stack-arguments).
