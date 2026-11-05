# Factorial and Fibonacci

Computes `factorial(8)` = 40320 and `fib(10)` = 55, except on X86 where the factorial is `factorial(10)` = 3628800. Build, then Run, and inspect the answers below.

Both subroutines call themselves. Each call saves the values it needs when the nested call returns. Factorial makes one recursive call at a time. Fibonacci makes two.

| Target | Where the answers are                   |
| ------ | --------------------------------------- |
| M68K   | `d6` and `d7`.                          |
| MIPS   | `$s0` and `$s1`.                        |
| RISC-V | `s0` and `s1`.                          |
| X86    | In memory, at `fact_out` and `fib_out`. |
| Z80    | `ix` and `hl`.                          |

The editor's courses explain it line by line: [M68K](https://asm-editor.specy.app/learn/courses/m68k/examples/factorial-and-fibonacci), [MIPS](https://asm-editor.specy.app/learn/courses/mips/examples/factorial-and-fibonacci), [RISC-V](https://asm-editor.specy.app/learn/courses/risc-v/examples/factorial-and-fibonacci), [X86](https://asm-editor.specy.app/learn/courses/x86/examples/factorial-and-fibonacci) and [Z80](https://asm-editor.specy.app/learn/courses/z80/examples/factorial-and-fibonacci).
