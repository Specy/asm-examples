# Sum of an array

Adds six numbers that the assembler places next to each other in memory. A register holds the address of the next one, and the loop moves it forward one element a pass. Build, then Run.

| Target | Where the sum is                                                                              |
| ------ | --------------------------------------------------------------------------------------------- |
| M68K   | `d0`, counting the passes in `d1`.                                                            |
| MIPS   | `$t2`.                                                                                        |
| RISC-V | `t2`. The loop stops when the pointer reaches the address just past the end, with no counter. |
| X86    | `rax`, walking from `rsi` to the end address in `rdi`.                                        |
| Z80    | `a`, the element in `hl`.                                                                     |

The editor's courses explain it line by line: [M68K](https://asm-editor.specy.app/learn/courses/m68k/examples/sum-of-an-array), [MIPS](https://asm-editor.specy.app/learn/courses/mips/examples/sum-of-an-array), [RISC-V](https://asm-editor.specy.app/learn/courses/risc-v/examples/sum-of-an-array), [X86](https://asm-editor.specy.app/learn/courses/x86/examples/sum-of-an-array) and [Z80](https://asm-editor.specy.app/learn/courses/z80/examples/sum-of-an-array).
