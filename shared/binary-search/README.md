# Binary search

This program looks for a number in a sorted array. It uses binary search to find the match.

The result is the index of the number in the array. If the number is not found, the result is -1.

| Target | Looks for | Result | Where the result is |
| ------ | --------- | ------ | ------------------- |
| M68K   | 91        | 9      | `d3`                |
| MIPS   | 91        | 9      | `$t4`               |
| RISC-V | 91        | 9      | `t4`                |
| X86    | 23        | 4      | `r8`                |
| Z80    | 91        | 9      | `e`, where -1 is represented as `0xFF` |

Go to the course's lecture to read a line-by-line explanation: [M68K](https://asm-editor.specy.app/learn/courses/m68k/examples/binary-search), [MIPS](https://asm-editor.specy.app/learn/courses/mips/examples/binary-search), [RISC-V](https://asm-editor.specy.app/learn/courses/risc-v/examples/binary-search), [X86](https://asm-editor.specy.app/learn/courses/x86/examples/binary-search) or [Z80](https://asm-editor.specy.app/learn/courses/z80/examples/binary-search).
