# Pac-Man

This game is the arcade game Pac-Man, with its sound, written twice: once in RISC-V assembly and once in C.

## Playing

1. Pick a version in the program picker, at the right of the bottom panel's tabs: `main.s` is the assembly version and `pacman.c` is the C version.
2. Click the Screen so it gets the keyboard, then press Space.
3. Move with **W A S D**, **Z Q S D**, or **8 4 5 6** on the number pad. **P** pauses.

## The pictures

Both versions read their pictures from `assets/` when they start: `sprites.bmp`, `tiles.bmp` and `maze.bmp`, which are 24-bit BMPs, and the maze layout in `maze.txt`. To redraw a picture, open it and choose **Pixels** under the editor. The next run uses what you drew. Black is transparent in `sprites.bmp`.

## The assembly version

`assembly/main.s` is the Entry file of the game.

| File          | What it holds                                                                |
| ------------- | ---------------------------------------------------------------------------- |
| `main.s`      | The Screen directive, the main loop and the `.include` lines                 |
| `constants.s` | Names for every number the program uses                                      |
| `data.s`      | The variables, and the arcade's level, speed and timing tables               |
| `assets.s`    | Reads the pictures and the maze with the file services (ecalls 1024, 63, 57) |
| `video.s`     | Drawing: tiles, text, numbers and sprites                                    |
| `maze.s`      | The maze as the game plays it                                                |
| `pacman.s`    | Pac-Man's movement and eating                                                |
| `ghosts.s`    | The ghosts: the house, their targets, frightened mode, collisions            |
| `game.s`      | The game's states, the score, the keyboard and the frame timing              |
| `sound.s`     | Arcade sound routines, music and effect data, and Synth controls             |

## The C version

| File       | What it holds                                                    |
| ---------- | ---------------------------------------------------------------- |
| `pacman.c` | `main`, the `SIM_SCREEN` line and the includes                   |
| `pacman.h` | The constants, types and prototypes the parts share              |
| `state.c`  | The variables, and the arcade's level, speed and timing tables   |
| `assets.c` | Reads the pictures and the maze                                  |
| `video.c`  | Drawing                                                          |
| `maze.c`   | The maze as the game plays it                                    |
| `player.c` | Pac-Man                                                          |
| `ghosts.c` | The ghosts                                                       |
| `game.c`   | The game's states, the score, the keyboard and the frame timing  |
| `sound.c`  | Arcade sound routines, music and effect data, and Synth controls |

## License

The program is under the MIT License, as the rest of asm-examples. Pac-Man itself is Bandai Namco's: the game it plays, the arcade's level, speed and timing tables in `data.s` and `state.c`, and the music and effect data of the arcade's sound program in `sound.s` and `sound.c` are the arcade's, and the MIT License does not cover them.
