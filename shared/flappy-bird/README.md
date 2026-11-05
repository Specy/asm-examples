# Flappy Bird

Fly between the pipes: every tap flaps, and gravity does the rest. Build, Run, click the Screen so that it has the keyboard, then tap Space to start and to flap.

All versions also accept Enter. MIPS and both RISC-V versions accept lowercase `w`. M68K and Z80 also accept the W key, the up arrow and the left mouse button. After a crash, tap once to reset and again to start. MIPS and RISC-V print the score and the session's best to the console at each game over.

| Target | Written in | How it works |
| ------ | ---------- | ------------ |
| M68K | Assembly | Draws off screen and shows each frame with task 94. Task 23 waits, task 19 reads the keys, task 61 reads the mouse and task 95 draws the score. |
| RISC-V | C | Uses `<sim.h>`. `SIM_SCREEN(display, 384, 512, 2)` makes a grid of 192 × 256 cells, each 2 pixels square, with the same picture as the assembly version. |
| MIPS | Assembly | A grid of 192 × 256 cells, one word of the bitmap display each: clouds, buildings and bushes worked out once, shaded pipes, and the bird and score drawn from sprites. Each frame paints only the pixels that change, each one once. |
| RISC-V | Assembly | As on MIPS, beside the C version in the same Project. |
| Z80 | Assembly | Pipes with gaps from a clock-seeded sequence. |

For RISC-V, `flappy-bird.s` is the program that opens. To run the C version, select the generated assembly, `flappy-bird.c.riscv`, as the program. It is already compiled at `-O2`. After changing `flappy-bird.c`, choose `-O2` and Compile it again before Build and Run. This enables compiler optimizations to help the game keep its intended 20 millisecond frame interval.

The bird and the score's digits are the sprites in `assets/bird.bmp` (the wing up, level and down, one above the other) and `assets/digits.bmp` (0 to 9). MIPS and RISC-V read them when the program starts; edit them in the Pixels view, then Build and Run again. Keep them 32-bit BMPs of the same size. Black is see-through.

Undo is off for the MIPS and RISC-V programs, which keeps the game running at full speed. Turn it on in the Program's settings to step back through a game.

On M68K, the editor's instruction execution limit may eventually stop a long game. Set **Instruction execution limit** to 0 in Settings to play without that limit.

The editor's courses go through both: [Flappy bird](https://asm-editor.specy.app/learn/courses/m68k/examples/flappy-bird) in M68K assembly, and [Flappy bird in C](https://asm-editor.specy.app/learn/courses/risc-v/examples/flappy-bird-in-c) for RISC-V.
