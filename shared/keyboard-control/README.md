# Keyboard control

This program moves a square that you steer with the keyboard. Click the Screen first, so the keys go to the program.

| Target | How you steer it |
| ------ | ---------------- |
| M68K   | The arrow keys set its direction. It keeps moving, and wraps around to the opposite edge. |
| MIPS   | Lowercase `w`, `a`, `s` and `d`, read from the memory-mapped keyboard. It keeps moving, and wraps around to the opposite edge. |
| RISC-V | Same as MIPS. |
| X86    | Hold an arrow key to move and let go to stop. The program reads key events from the input device. Escape ends the program, and so do ten seconds without a key. |
| Z80    | Same as M68K. |

Go to the course's lecture to read a line-by-line explanation: [M68K](https://asm-editor.specy.app/learn/courses/m68k/examples/keyboard-control), [MIPS](https://asm-editor.specy.app/learn/courses/mips/examples/keyboard-control), [RISC-V](https://asm-editor.specy.app/learn/courses/risc-v/examples/keyboard-control), [X86](https://asm-editor.specy.app/learn/courses/x86/examples/keyboard-control) or [Z80](https://asm-editor.specy.app/learn/courses/z80/examples/keyboard-control).
