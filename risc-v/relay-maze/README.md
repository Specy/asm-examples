# Relay Maze

Clear every dot from the maze while avoiding four pursuers. The larger purple power-ups briefly turn the tables: catch vulnerable enemies for extra points. Defeated enemies return to the central pen before joining the chase again.

You start with three lives. Collecting every dot starts the next level. Enemies get faster and power-ups get shorter over the first six levels. Bonus gems appear during play, and reaching 10,000 points earns an extra life.

Background music plays as you move around the maze. Power-ups switch to a faster tune until the enemies are no longer vulnerable. Pausing the game also pauses the music.

## Playing

1. Pick a version in the program picker: `main.s` is the hand-written assembly version and `relay.c` is the C version.
2. Click the Screen so it gets the keyboard, then press **Space** to start.
3. Move with **W A S D**, **Z Q S D**, or **8 4 5 6** on the number pad. Your character keeps moving until a wall stops it. Choose a direction before a junction to take the turn as soon as it opens.
4. **P** pauses or resumes during play. The side passages wrap to the opposite edge of the maze.

Small dots are worth 10 points, power-ups 50. Catching enemies during one power-up scores 200, 400, 800 and 1,600 points. The score and high score appear above the maze, with spare lives and recent bonus gems below it.

## Changing the game

Both versions read the same files from `assets/` when they start. Open a BMP and choose **Pixels** under the editor to draw on it. Build and run again to see your changes.

- `sprites.bmp` has sixteen columns and nine rows of 16 by 16 cells. Row 0 holds three poses (idle and two walking poses) for each direction, ordered right, left, up and down. Row 1 holds the collision animation, rows 2 to 5 the four enemies, row 6 the vulnerable and returning enemies, row 7 the bonus gems, and row 8 the point values. Black pixels are transparent.
- `tiles.bmp` has sixteen columns and five rows of 8 by 8 cells. The first four rows are ASCII characters 32 to 95. The last row starts with the dot and power-up.
- `maze.bmp` is the 224 by 248 picture of the walls, displayed below the score.
- `maze.txt` has 31 rows of 28 characters. `#` is a wall, `.` a dot, `o` a power-up, a space an empty corridor, and `-` the pen door. Change the picture and the layout together. Keep the central pen, starting positions and side passages at their current coordinates, since movement into and out of the pen follows fixed paths.

The repository's `scripts/relay-maze-assets.mjs` generates the original pixel art, lettering and maze. Running it again replaces edits to these assets.

## Source files

| Assembly | C | What it does |
| --- | --- | --- |
| `main.s` | `relay.c` | Configures the Screen, loads the assets and runs the frame loop |
| `constants.s`, `data.s` | `relay.h`, `state.c` | Constants, state, buffers and difficulty settings |
| `assets.s` | `assets.c` | Reads the files and decodes BMP pictures |
| `video.s` | `video.c` | Draws the maze, text and sprites |
| `maze.s` | `maze.c` | Keeps track of dots and power-ups |
| `player.s` | `player.c` | Movement, cornering and collecting items |
| `enemies.s` | `enemies.c` | Pursuit, vulnerability, returning to the pen and collisions |
| `game.s` | `game.c` | Game states, scoring, lives, keyboard input and frame timing |
| `sound.s` | `sound.c` | Original melodies and sound cues on the Synth |

Compile `c/relay.c` after changing its C sources, then build and run the generated assembly. The assembly version can be built directly from `assembly/main.s`.

## License

The source code, graphics, lettering, maze and sound cues in this example are provided under the repository's MIT License.
