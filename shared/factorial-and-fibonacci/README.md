# Factorial and Fibonacci

Computes `factorial(8)` = 40320 and `fib(10)` = 55 with subroutines that call themselves. Each call saves on the stack what it still needs, so it can wait while the call it made works. Factorial makes a chain of calls; Fibonacci splits each one into two. Build, then Run.

| Target | Where the answers are                   |
| ------ | --------------------------------------- |
| M68K   | `d6` and `d7`.                          |
| MIPS   | `$s0` and `$s1`.                        |
| RISC-V | `s0` and `s1`.                          |
| X86    | In memory, at `fact_out` and `fib_out`. |
| Z80    | `ix` and `hl`.                          |

The editor's courses explain it line by line: [M68K](https://asm-editor.specy.app/learn/courses/m68k/examples/factorial-and-fibonacci), [MIPS](https://asm-editor.specy.app/learn/courses/mips/examples/factorial-and-fibonacci), [RISC-V](https://asm-editor.specy.app/learn/courses/risc-v/examples/factorial-and-fibonacci), [X86](https://asm-editor.specy.app/learn/courses/x86/examples/factorial-and-fibonacci) and [Z80](https://asm-editor.specy.app/learn/courses/z80/examples/factorial-and-fibonacci).
