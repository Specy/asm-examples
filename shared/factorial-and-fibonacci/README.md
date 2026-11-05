# Factorial and Fibonacci

This program computes a factorial and a Fibonacci number using recursion: each subroutine calls itself. Before a call, each subroutine saves the values it still needs after the call returns.

| Target | Results | Where the results are |
| ------ | ------- | --------------------- |
| M68K   | `factorial(8)` = 40320, `fib(10)` = 55 | `d6` and `d7` |
| MIPS   | `factorial(8)` = 40320, `fib(10)` = 55 | `$s0` and `$s1` |
| RISC-V | `factorial(8)` = 40320, `fib(10)` = 55 | `s0` and `s1` |
| X86    | `factorial(10)` = 3628800, `fib(10)` = 55 | In memory, at `fact_out` and `fib_out` |
| Z80    | `factorial(8)` = 40320, `fib(10)` = 55 | `ix` and `hl` |

Go to the course's lecture to read a line-by-line explanation: [M68K](https://asm-editor.specy.app/learn/courses/m68k/examples/factorial-and-fibonacci), [MIPS](https://asm-editor.specy.app/learn/courses/mips/examples/factorial-and-fibonacci), [RISC-V](https://asm-editor.specy.app/learn/courses/risc-v/examples/factorial-and-fibonacci), [X86](https://asm-editor.specy.app/learn/courses/x86/examples/factorial-and-fibonacci) or [Z80](https://asm-editor.specy.app/learn/courses/z80/examples/factorial-and-fibonacci).
