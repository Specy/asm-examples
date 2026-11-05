# Snake

This game is Snake: steer the snake to the food. Each piece of food scores a point and makes the snake one segment longer. The game ends when the head hits a wall or the snake's own body. Click the Screen first, so the keys go to the program. Press Run again for a new game.

| Target | How it plays |
| ------ | ------------ |
| M68K   | A board of 32 × 24 cells, steered with the arrow keys. Each frame is drawn off screen and shown whole. The score is drawn on the Screen, and the final score is printed to the console. |
| MIPS   | A board of 32 × 32 cells, one word of the bitmap display per cell, steered with lowercase `w`, `a`, `s` and `d`. The `@screen` comment at the top connects the board to the Screen. The board turns dark red when the game ends. |
| RISC-V | Same as MIPS. |
| Z80    | A board of 15 × 11 cells, steered with the arrow keys. The score is shown above the board, and the final score is printed to the console. |

Go to the course's lecture to read how the game is built, one piece at a time: [M68K](https://asm-editor.specy.app/learn/courses/m68k/examples/snake), [MIPS](https://asm-editor.specy.app/learn/courses/mips/examples/snake), [RISC-V](https://asm-editor.specy.app/learn/courses/risc-v/examples/snake) or [Z80](https://asm-editor.specy.app/learn/courses/z80/examples/snake).
