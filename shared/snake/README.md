# Snake

Steer the snake to the food: every piece eaten scores a point and makes it one segment longer, and the game ends when the head hits a wall or the snake's body. Build, Run, then click the Screen so that the keys go to the program. Press Run again for a new game.

| Target | How it plays                                                                                                                                                                                                           |
| ------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| M68K   | A board of 32 × 24 cells, steered with the arrow keys. Each frame is drawn off screen and shown whole. The score is drawn on the Screen and the final one goes to the console.                                         |
| MIPS   | A board of 32 × 32 cells, one word of the bitmap display each, steered with lowercase `w`, `a`, `s` and `d`. The `@screen` comment at the top connects the board to the Screen. The board turns dark red on game over. |
| RISC-V | As on MIPS.                                                                                                                                                                                                            |
| Z80    | A board of 15 × 11 cells, steered with the arrow keys. The score sits above the board and the last one goes to the console.                                                                                            |

The editor's courses go through it a piece at a time: [Snake](https://asm-editor.specy.app/learn/courses/m68k/examples/snake) for M68K, and the same lecture for [MIPS](https://asm-editor.specy.app/learn/courses/mips/examples/snake), [RISC-V](https://asm-editor.specy.app/learn/courses/risc-v/examples/snake) and [Z80](https://asm-editor.specy.app/learn/courses/z80/examples/snake).
