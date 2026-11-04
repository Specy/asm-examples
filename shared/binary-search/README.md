# Binary search

Looks for 91 in a sorted array of twelve numbers. Each comparison with the middle element rules out half of what is left, so it checks four elements where a search from the left would check ten. It leaves the index it found, or -1 when the value is absent. Build, then Run.

| Target | Where the index is |
| ------ | ------------------ |
| M68K   | `d3`.              |
| MIPS   | `$t4`.             |
| RISC-V | `t4`.              |
| X86    | `r8`.              |
| Z80    | `e`.               |

The editor's courses explain it line by line: [M68K](https://asm-editor.specy.app/learn/courses/m68k/examples/binary-search), [MIPS](https://asm-editor.specy.app/learn/courses/mips/examples/binary-search), [RISC-V](https://asm-editor.specy.app/learn/courses/risc-v/examples/binary-search), [X86](https://asm-editor.specy.app/learn/courses/x86/examples/binary-search) and [Z80](https://asm-editor.specy.app/learn/courses/z80/examples/binary-search).
