# Bubble sort

Sorts an array from smallest to largest in the same memory. Each pass compares neighbours and swaps a pair that is out of order, which carries the largest remaining value to the end. The next pass stops one pair earlier. Build, Run, and look at the array in the memory panel.

| Target | What it sorts                   |
| ------ | ------------------------------- |
| M68K   | Eight signed words.             |
| MIPS   | Eight words.                    |
| RISC-V | Eight words, at `numbers`.      |
| X86    | Six signed qwords, at `values`. |
| Z80    | Eight unsigned bytes.           |

The editor's courses explain it line by line: [M68K](https://asm-editor.specy.app/learn/courses/m68k/examples/bubble-sort), [MIPS](https://asm-editor.specy.app/learn/courses/mips/examples/bubble-sort), [RISC-V](https://asm-editor.specy.app/learn/courses/risc-v/examples/bubble-sort), [X86](https://asm-editor.specy.app/learn/courses/x86/examples/bubble-sort) and [Z80](https://asm-editor.specy.app/learn/courses/z80/examples/bubble-sort).
