# Number to string

Prints a number in hexadecimal, decimal and binary. Build, then Run, and read the console.

| Target | Console output, in order |
| ------ | ------------------------ |
| M68K, MIPS, RISC-V, Z80 | `BEEF`, `48879`, `1011111011101111`: the value 48879 in bases 16, 10 and 2. |
| X86 | `12345`, `3039`, `11000000111001`: the value 12345 in bases 10, 16 and 2. |

The program divides by the base to find each digit and turns it into its character, then prints the finished string, without any service that prints numbers.

The editor's courses explain it line by line: [M68K](https://asm-editor.specy.app/learn/courses/m68k/examples/number-to-string), [MIPS](https://asm-editor.specy.app/learn/courses/mips/examples/number-to-string), [RISC-V](https://asm-editor.specy.app/learn/courses/risc-v/examples/number-to-string), [X86](https://asm-editor.specy.app/learn/courses/x86/examples/number-to-string) and [Z80](https://asm-editor.specy.app/learn/courses/z80/examples/number-to-string).
