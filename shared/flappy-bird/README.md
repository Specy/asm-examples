# Flappy Bird

Fly between the pipes: every tap flaps, and gravity does the rest. Build, Run, click the Screen so that it has the keyboard, then tap Space to start and to flap.

Both versions also accept W and Enter. On M68K, the up arrow and the left mouse button flap too. After a crash, tap once to reset and again to start. RISC-V prints the score and the session's best to the console.

| Target | Written in | How it works |
| ------ | ---------- | ------------ |
| M68K | Assembly | Draws off screen and shows each frame with task 94. Task 23 waits, task 19 reads the keys, task 61 reads the mouse and task 95 draws the score. |
| RISC-V | C | Uses `<sim.h>`. `SIM_SCREEN(display, 256, 256, 4)` makes a grid of 64 × 64 cells, each 4 pixels square. |
| MIPS | Assembly | A grid of 64 × 64 cells, one word of the bitmap display each. Space, `w` and Enter flap, and the score goes to the console. |
| RISC-V | Assembly | As on MIPS, beside the C version in the same Project. |
| Z80 | Assembly | Pipes with gaps from a clock-seeded sequence. A mouse click flaps too. |

For RISC-V, `flappy-bird.s` is the program that opens. To run the C version, select the generated assembly, `flappy-bird.c.riscv`, as the program. It is already compiled at -O0. After changing `flappy-bird.c`, Compile it again before Build and Run.

On M68K, the editor's instruction execution limit may eventually stop a long game. Set **Instruction execution limit** to 0 in Settings to play without that limit.

The editor's courses go through both: [Flappy bird](https://asm-editor.specy.app/learn/courses/m68k/examples/flappy-bird) in M68K assembly, and [Flappy bird in C](https://asm-editor.specy.app/learn/courses/risc-v/examples/flappy-bird-in-c) for RISC-V.
