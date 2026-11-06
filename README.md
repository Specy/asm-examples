# asm-examples

Example programs for [asm-editor](https://github.com/Specy/asm-editor), the web IDE for assembly. An example is written for one Target or for several. Each version of it is one Project, which the build packs into an `.asmproj` archive that the editor opens.

<!-- examples:start -->
| Example | Targets | What it is | License |
| ------- | ------- | ---------- | ------- |
| [asmOS](shared/asmos/) | MIPS, RISC-V, X86 | A desktop with a process scheduler and window manager, written in C and x86 assembly. Includes Pac-Man, DOOM and a RISC-V assembly editor. | GPL-2.0-or-later |
| [Bad Apple!!](m68k/bad-apple/) | M68K | Bad Apple!!, the shadow-play video, as 6,526 frames of 16 by 16 pixels drawn with rectangles. | MIT |
| [Binary search](shared/binary-search/) | M68K, MIPS, RISC-V, X86, Z80 | Finds a value in a sorted array by halving the search range. | MIT |
| [Bitmap tour](shared/bitmap-tour/) | MIPS, RISC-V | The bitmap display, one word of memory a pixel: a colour ramp, a square and a border over a 256 by 256 grid. | MIT |
| [Bouncing ball](shared/bouncing-ball/) | M68K, MIPS, RISC-V, X86, Z80 | A ball bouncing around the Screen, with a wait between frames. | MIT |
| [Branches and loops](shared/branches-and-loops/) | M68K, MIPS, RISC-V, X86, Z80 | Compares two numbers with if/else, then writes the numbers 1 to 10 into an array. | MIT |
| [Bubble sort](shared/bubble-sort/) | M68K, MIPS, RISC-V, X86, Z80 | Sorts an array in place by swapping neighbours that are out of order. | MIT |
| [Chiptune](shared/chiptune/) | M68K, MIPS, RISC-V, Z80 | Plays a melody, bass line and drums on three sound channels. | MIT |
| [Drawing a house](shared/drawing-a-house/) | M68K, MIPS, RISC-V, X86, Z80 | Draws a house, sky and sun on the Screen. | MIT |
| [Factorial and Fibonacci](shared/factorial-and-fibonacci/) | M68K, MIPS, RISC-V, X86, Z80 | Computes factorials and Fibonacci numbers with subroutines that call themselves. | MIT |
| [Flappy ASM](shared/flappy-asm/) | M68K, MIPS, RISC-V, Z80 | Fly between the pipes: every tap flaps, and gravity does the rest. In assembly and, for RISC-V, in C as well. | MIT |
| [Graphics tour](m68k/graphics-tour/) | M68K | Every drawing task of the M68K screen on one picture, then the sound tasks, ported from EASy68K's own example. | GPL-2.0-or-later |
| [Hello world](shared/hello-world/) | M68K, MIPS, RISC-V, X86, Z80 | Prints a greeting and a number to the console. | MIT |
| [Keyboard and display](shared/keyboard-display/) | MIPS, RISC-V | Echo typed characters through the memory-mapped display registers and paint cells of the bitmap display. | MIT |
| [Keyboard control](shared/keyboard-control/) | M68K, MIPS, RISC-V, X86, Z80 | A square on the Screen that you steer with the keyboard. | MIT |
| [Keyboard move](shared/keyboard-move/) | M68K, Z80 | A square that moves while the arrow keys are held, reading which keys are down as a game does. | MIT |
| [Mouse paint](shared/mouse-paint/) | M68K, X86, Z80 | Painting with the mouse: a brush under the pointer while the left button is held, red with Shift. | MIT |
| [Number to string](shared/number-to-string/) | M68K, MIPS, RISC-V, X86, Z80 | Converts a number to hexadecimal, decimal and binary text, then prints it. | MIT |
| [PC speaker](x86/pc-speaker/) | X86 | The opening of Für Elise on the PC speaker, one square-wave tone at a time. | MIT |
| [Play a tune](shared/play-a-tune/) | MIPS, RISC-V, X86, Z80 | The first phrase of Ode to Joy, played from a table of notes. | MIT |
| [Play sound files](m68k/play-sound-files/) | M68K | Writes a WAV file and a MIDI file with the file tasks, then plays both. | MIT |
| [Relay Maze](risc-v/relay-maze/) | RISC-V | Clear the dots, dodge four pursuers and turn the tables with power-ups. An arcade maze-chase game in RISC-V assembly and C. | MIT |
| [Reverse a string](shared/reverse-a-string/) | M68K, MIPS, RISC-V, X86, Z80 | Reverses a zero-terminated string in place using two pointers. | MIT |
| [Sampled sound](shared/sampled-sound/) | M68K, MIPS, RISC-V, X86 | A sound computed sample by sample, then played as a scale. | MIT |
| [Snake](shared/snake/) | M68K, MIPS, RISC-V, Z80 | Steer the snake to the food to grow it, without hitting a wall or its own body. | MIT |
| [Subroutine with stack arguments](shared/subroutine-with-stack-arguments/) | M68K, MIPS, RISC-V, X86, Z80 | Passes arguments on the stack and keeps a local variable in a stack frame. | MIT |
| [Sum of an array](shared/sum-of-an-array/) | M68K, MIPS, RISC-V, X86, Z80 | Adds six numbers from an array using a pointer. | MIT |
| [Sum of two numbers](shared/sum-of-two-numbers/) | M68K, MIPS, RISC-V, Z80 | Asks for two numbers in the console and prints their sum. | MIT |
| [TRS-80 bounce](z80/trs80-bounce/) | Z80 | A block bouncing around the TRS-80's display, each frame composed in memory and copied in with one ldir. | MIT |
| [TRS-80 text](z80/trs80-text/) | Z80 | Text and block graphics stored straight into the TRS-80's video memory. | MIT |
<!-- examples:end -->

## Layout

```
m68k/ mips/ risc-v/ risc-v-64/ x86/ z80/
  <example>/            an example for that Target: the folder is its Project
shared/
  <example>/            an example for several Targets
    mips/ risc-v/ …     each Target folder is one Project
    README.md, assets/  every other File goes into each of them
```

An example's folder name is its id, unique in the repository. When an example for one Target is ported to another, it moves from `risc-v/<example>/` to `shared/<example>/risc-v/`.

The editor identifies each port by `<target-folder>-<id>[-<variant>]`, for example `mips-bouncing-ball`. Keep the id and variant unchanged when changing a display name, since they are part of its shared URLs.

## Building

```sh
npm ci
npm run build
```

`dist/<target>/<example>.asmproj` is one archive for each version of each example. `dist/index.json` lists every example with its archives (`examples`), and the examples of every Target (`targets`). The build also rewrites the table above. It fails, writing nothing, when something is wrong: a missing File, a Project over the editor's limits (64 MiB, 4,096 Files), a text File that is not UTF-8, or Generated assembly that is no longer what its C source compiles to. `npm run check` is the build that CI runs: it also fails when the table is out of date. `npm test` tests the build itself.

## Adding an example

Every example folder has an `example.json`:

```json
{
    "name": "Bouncing ball",
    "description": "A ball that bounces, drawn off screen and shown a frame at a time.",
    "tags": ["screen", "animation"]
}
```

That is enough for an example of one assembly File. It becomes the Entry of the Project's one Program and the tab that opens, beside the `README.md` if there is one. Anything else uses the editor's own field names, in `project`, or for a shared example in `ports.<target>.project`:

```json
{
    "name": "Relay Maze",
    "description": "Clear the dots, dodge four pursuers and turn the tables with power-ups.",
    "project": {
        "programs": {
            "assembly/main.s": { "display": { "width": 224, "height": 288 } },
            "c/relay.c.riscv": {}
        },
        "selectedProgram": "assembly/main.s",
        "openTabs": ["README.md", "assembly/main.s", "c/relay.c"],
        "activeTab": "assembly/main.s",
        "fileViews": { "README.md": "preview" }
    }
}
```

A shared example's `ports.<target>` can also give that version its own `name` and `description`. `example.json`, `compilations.json` and `build.mjs` are never packed.

`startup` says what opening the example does, as the flags a link to it can carry: `build` builds it, `buildAndRun` builds and runs it, and `fullscreenDisplay` opens its Screen in the floating window. A game starts running on a display that fills the page with:

```json
"startup": { "buildAndRun": true, "fullscreenDisplay": true }
```

The editor's Examples page opens the example with these flags, and so does its Open in editor link; a link to the Examples page that sets any of these flags decides them instead. A link straight to the example in the editor starts it only as its own flags say. A version's own `startup`, in `ports.<target>` or as an option of `pack()`, replaces the example's, and `{}` opens that version with none.

### Changing an example in the editor

Open its archive from `dist/`, change it, export it as an `.asmproj`, then bring it back:

```sh
npm run sync -- ~/Downloads/Relay-Maze.asmproj relay-maze
```

`sync` writes back the Files that changed or were added; with `--delete` it also removes those the Project no longer has. It never changes `example.json`. Where the exported Programs, tabs or File views differ from what `example.json` gives, it prints the difference, so you can copy what you want to keep. A Project exported for a Target the shared example has no folder for becomes a new version of it.

### C and C++

A C example ships its Generated assembly as a File, so it runs without compiling, together with the editor's Compilation record, which says which sources it was compiled from. `sync` keeps the records in `compilations.json`. The build checks them against the Files and fails once a source has changed since it was compiled: Compile again in the editor, export, sync.

### A build of its own

A `build.mjs` in an example folder replaces the default, for an example whose versions share one tree, as asmOS's do:

```js
export default async function build({ files, pack }) {
    pack({
        target: 'X86',
        files: files({ include: ['os/', 'games/', 'README.md'], exclude: ['os/platform/riscv/'] }),
        project: { programs: { 'os/main.c.asm': { settings: { undoEnabled: false } } } },
        buildOutputs: ['os/main.c.asm']
    })
}
```

- **`files({ from, include, exclude })`** returns the example's Files, or those of its `from` subfolder. Each is listed as its path in the archive, mapped to the file it is read from. An entry ending in `/` is a folder.
- **`pack({ target, files, project, name, description, variant, buildOutputs, startup, draft })`** packs one archive. A `variant` tells two archives for one Target apart: `<example>-<variant>.asmproj`. Set `draft: true` while a port is unfinished. Its sources and Compilation records stay available to compilation tools, and the build leaves it out of the archives and index.
- **`buildOutputs`** lists the Programs whose linked program the archive should carry. The editor adds it when it builds the examples, because only its Core can link one, and only x86 saves one.

## License

The examples are under the MIT License ([LICENSE](LICENSE)), except those whose folder has a license of its own and says so in its `example.json`: asmOS is under the GNU General Public License, version 2 or later. Third-party material keeps its own terms, which each example's README and NOTICE name.
