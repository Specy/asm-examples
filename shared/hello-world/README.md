# Hello world

This program prints a greeting and a number to the console. Use it as a starting point for your own programs: put the text in memory, then ask the system to print it.

| Target | How it prints |
| ------ | ------------- |
| M68K   | With the tasks of `trap #15`. The task number goes in `d0`. |
| MIPS   | With `syscall`. The service number goes in `$v0`. |
| RISC-V | With `ecall`. The service number goes in `a7`. It prints text, a number and a character on one line. |
| X86    | With the Linux `write` system call, through `syscall`, once per line. |
| Z80    | With `out` to the console ports: one port prints a byte as a character, the other as a decimal number. |

Go to the course's lecture to read a line-by-line explanation: [M68K](https://asm-editor.specy.app/learn/courses/m68k/examples/hello-world), [MIPS](https://asm-editor.specy.app/learn/courses/mips/examples/hello-world), [RISC-V](https://asm-editor.specy.app/learn/courses/risc-v/examples/hello-world), [X86](https://asm-editor.specy.app/learn/courses/x86/examples/hello-world) or [Z80](https://asm-editor.specy.app/learn/courses/z80/examples/hello-world).
