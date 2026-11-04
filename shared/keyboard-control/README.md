# Keyboard control

A square you steer with the keyboard. Build, Run, then click the Screen so that the keys go to the program.

| Target | How it steers                                                                                                                                     |
| ------ | ------------------------------------------------------------------------------------------------------------------------------------------------- |
| M68K   | The arrow keys set its direction. It keeps moving, and comes back in at the opposite edge.                                                        |
| MIPS   | Lowercase `w`, `a`, `s` and `d`, read from the memory-mapped keyboard. It keeps moving, and comes back in at the opposite edge.                   |
| RISC-V | As on MIPS.                                                                                                                                       |
| X86    | Hold an arrow key to move and let go to stop, read as key events from the input device. Escape ends the program, as do ten seconds without a key. |
| Z80    | As on M68K.                                                                                                                                       |

The editor's courses explain it line by line: [M68K](https://asm-editor.specy.app/learn/courses/m68k/examples/keyboard-control), [MIPS](https://asm-editor.specy.app/learn/courses/mips/examples/keyboard-control), [RISC-V](https://asm-editor.specy.app/learn/courses/risc-v/examples/keyboard-control), [X86](https://asm-editor.specy.app/learn/courses/x86/examples/keyboard-control) and [Z80](https://asm-editor.specy.app/learn/courses/z80/examples/keyboard-control).
