# asm-examples

Example programs for [asm-editor](https://github.com/Specy/asm-editor), the web IDE for assembly. An example is written for one Target or for several. Each version of it is one Project, which the build packs into an `.asmproj` archive that the editor opens.

<!-- examples:start -->
| Example | Targets | What it is | License |
| ------- | ------- | ---------- | ------- |
| [asmOS](shared/asmos/) | MIPS, RISC-V, X86 | A small operating system with processes, a compositing window manager and eighteen applications, Pac-Man, DOOM, a Video Player playing Bad Apple!! and an Assembly Editor that runs RISC-V programs among them, in C and x86 assembly. | GPL-2.0-or-later |
| [Bad Apple!!](m68k/bad-apple/) | M68K | Bad Apple!!, the shadow-play video, as 6,526 frames of 16 by 16 pixels drawn with rectangles. | MIT |
| [Bitmap tour](shared/bitmap-tour/) | MIPS, RISC-V | The bitmap display, one word of memory a pixel: a colour ramp, a square and a border over a 256 by 256 grid. | MIT |
| [Bouncing ball](shared/bouncing-ball/) | M68K, MIPS, RISC-V, Z80 | A ball bouncing around the Screen, drawn a whole frame at a time and paced by the clock. | MIT |
| [Flappy Bird](shared/flappy-bird/) | M68K, RISC-V | Fly between the pipes: every tap flaps, and gravity does the rest. In M68K assembly and, for RISC-V, in C. | MIT |
| [Graphics tour](m68k/graphics-tour/) | M68K | Every drawing task of the M68K screen on one picture, then the sound tasks, ported from EASy68K's own example. | GPL-2.0-or-later |
| [Keyboard and display](shared/keyboard-display/) | MIPS, RISC-V | The memory-mapped keyboard and display registers: every key typed is echoed to the console and paints a cell of the bitmap display. | MIT |
| [Keyboard move](shared/keyboard-move/) | M68K, Z80 | A square that moves while the arrow keys are held, reading which keys are down as a game does. | MIT |
| [Mouse paint](shared/mouse-paint/) | M68K, Z80 | Painting with the mouse: a brush under the pointer while the left button is held, red with Shift. | MIT |
| [Pac-Man](risc-v/pacman/) | RISC-V | The arcade game with its sound, in RISC-V assembly and in C, reading its pictures from BMP Files. | MIT |
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

`dist/<target>/<example>.asmproj` is one archive for each version of each example. `dist/index.json` lists every example with its archives (`examples`), and the examples of every Target (`targets`). The build also rewrites the table above. It fails, writing nothing, when something is wrong: a missing File, a Project over the editor's limits (16 MiB, 4,096 Files), a text File that is not UTF-8, or Generated assembly that is no longer what its C source compiles to. `npm run check` is the build that CI runs: it also fails when the table is out of date. `npm test` tests the build itself.

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
    "name": "Pac-Man",
    "description": "The arcade game with its sound, in RISC-V assembly and in C.",
    "project": {
        "programs": {
            "assembly/main.s": { "display": { "width": 224, "height": 288 } },
            "c/pacman.c.riscv": {}
        },
        "selectedProgram": "assembly/main.s",
        "openTabs": ["README.md", "assembly/main.s", "c/pacman.c"],
        "activeTab": "assembly/main.s",
        "fileViews": { "README.md": "preview" }
    }
}
```

A shared example's `ports.<target>` can also give that version its own `name` and `description`. `example.json`, `compilations.json` and `build.mjs` are never packed.

### Changing an example in the editor

Open its archive from `dist/`, change it, export it as an `.asmproj`, then bring it back:

```sh
npm run sync -- ~/Downloads/Pac-Man.asmproj pacman
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
- **`pack({ target, files, project, name, description, variant, buildOutputs, draft })`** packs one archive. A `variant` tells two archives for one Target apart: `<example>-<variant>.asmproj`. Set `draft: true` while a port is unfinished. Its sources and Compilation records stay available to compilation tools, and the build leaves it out of the archives and index.
- **`buildOutputs`** lists the Programs whose linked program the archive should carry. The editor adds it when it builds the examples, because only its Core can link one, and only x86 saves one.

## License

The examples are under the MIT License ([LICENSE](LICENSE)), except those whose folder has a license of its own and says so in its `example.json`: asmOS is under the GNU General Public License, version 2 or later. Third-party material keeps its own terms, which each example's README and NOTICE name.
