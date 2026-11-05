# Hello world

Prints a greeting and a number to the console. Build, then Run, and read the console. You can use it as a starting point for a new program: put the text in memory, then ask the system to print it.

| Target | How it prints                                                                                |
| ------ | -------------------------------------------------------------------------------------------- |
| M68K   | Tasks of `trap #15`, the task number in `d0`.                                                |
| MIPS   | `syscall`, the service number in `$v0`.                                                      |
| RISC-V | `ecall`, the service number in `a7`: text, a number and a character on one line.             |
| X86    | The Linux `write` system call, one for each line, through `syscall`.                         |
| Z80    | `out` to the console ports: one prints a byte as a character, the other as a decimal number. |

The editor's courses explain it line by line: [M68K](https://asm-editor.specy.app/learn/courses/m68k/examples/hello-world), [MIPS](https://asm-editor.specy.app/learn/courses/mips/examples/hello-world), [RISC-V](https://asm-editor.specy.app/learn/courses/risc-v/examples/hello-world), [X86](https://asm-editor.specy.app/learn/courses/x86/examples/hello-world) and [Z80](https://asm-editor.specy.app/learn/courses/z80/examples/hello-world).
