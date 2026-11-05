# Sum of an array

This program adds up an array of six numbers, using a pointer. A register holds the address of the next number, and each iteration of the loop moves it forward by one element.

| Target | Where the sum is |
| ------ | ---------------- |
| M68K   | `d0`. `d1` counts the iterations. |
| MIPS   | `$t2`. |
| RISC-V | `t2`. The loop has no counter: it stops when the pointer reaches the address just past the end. |
| X86    | `r8`. During the loop, `rsi` moves towards the end address in `rdi`. `r9` keeps a copy of that address. |
| Z80    | `a`. During the loop, `hl` holds the address of the next byte to add. |

Go to the course's lecture to read a line-by-line explanation: [M68K](https://asm-editor.specy.app/learn/courses/m68k/examples/sum-of-an-array), [MIPS](https://asm-editor.specy.app/learn/courses/mips/examples/sum-of-an-array), [RISC-V](https://asm-editor.specy.app/learn/courses/risc-v/examples/sum-of-an-array), [X86](https://asm-editor.specy.app/learn/courses/x86/examples/sum-of-an-array) or [Z80](https://asm-editor.specy.app/learn/courses/z80/examples/sum-of-an-array).
