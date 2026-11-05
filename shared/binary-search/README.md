# Binary search

Looks for 91 in a sorted array of twelve numbers, or 23 in six numbers on X86. Each comparison with the middle element rules out half of the remaining range. Build, then Run. The result is an index counting from 0: 9, or 4 on X86. An absent value returns -1, represented as `0xFF` (255 unsigned) in Z80's `e`.

| Target | Where the index is |
| ------ | ------------------ |
| M68K   | `d3`.              |
| MIPS   | `$t4`.             |
| RISC-V | `t4`.              |
| X86    | `r8`.              |
| Z80    | `e`.               |

The editor's courses explain it line by line: [M68K](https://asm-editor.specy.app/learn/courses/m68k/examples/binary-search), [MIPS](https://asm-editor.specy.app/learn/courses/mips/examples/binary-search), [RISC-V](https://asm-editor.specy.app/learn/courses/risc-v/examples/binary-search), [X86](https://asm-editor.specy.app/learn/courses/x86/examples/binary-search) and [Z80](https://asm-editor.specy.app/learn/courses/z80/examples/binary-search).
