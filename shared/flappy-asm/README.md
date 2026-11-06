# Flappy ASM

This game is Flappy ASM: fly between the pipes. Each tap flaps, and gravity does the rest. Click the Screen so it gets the keyboard, then press Space to start and to flap.

Every version also accepts Enter. MIPS and both RISC-V versions also accept lowercase `w`. M68K and Z80 also accept the W key, the up arrow and the left mouse button. After a crash, tap once to reset and again to start. MIPS and RISC-V print the score and the session's best score to the console when the game ends.

| Target | Written in | How it works |
| ------ | ---------- | ------------ |
| M68K | Assembly | Draws off screen and shows each frame with task 94. Task 23 waits, task 19 reads the keys, task 61 reads the mouse and task 95 draws the score. |
| MIPS | Assembly | Draws on a grid of 192 × 256 cells, one word of the bitmap display per cell. The clouds, buildings and bushes are computed once at the start; the pipes are shaded, and the bird and the score are drawn from sprites. Each frame paints only the pixels that change, and each of them only once. |
| RISC-V | Assembly | Same as MIPS, in the same Project as the C version. |
| RISC-V | C | Uses `<sim.h>`. `SIM_SCREEN(display, 384, 512, 2)` makes a grid of 192 × 256 cells, each 2 pixels square. It draws the same picture as the assembly version. |
| Z80 | Assembly | Places the gaps in the pipes from a sequence seeded by the clock. |

## The C version on RISC-V

The RISC-V Project opens with the assembly version, `flappy-asm.s`. To play the C version, pick `flappy-asm.c.riscv` in the program picker: it is `flappy-asm.c` already compiled with `-O2`. If you change `flappy-asm.c`, compile it again with `-O2`. The optimizations help the game keep up with its 20 ms frames.

## The sprites

The bird and the score's digits are sprites, in `assets/bird.bmp` (the wing up, level and down, one above the other) and `assets/digits.bmp` (0 to 9). MIPS and RISC-V read them when the program starts. To change them, open them in the Pixels view, then run the program again. Keep them 32-bit BMPs of the same size. Black is transparent.

## Speed

Undo is off for the MIPS and RISC-V programs, so the game runs at full speed. To step back through a game, turn Undo on in the Program's settings.

On M68K, the editor's instruction execution limit can stop a long game. To play without the limit, set **Instruction execution limit** to 0 in Settings.

Go to the course's lecture to read how the game is built: [Flappy ASM](https://asm-editor.specy.app/learn/courses/m68k/examples/flappy-asm) in M68K assembly, or [Flappy ASM in C](https://asm-editor.specy.app/learn/courses/risc-v/examples/flappy-asm-in-c) for RISC-V.
