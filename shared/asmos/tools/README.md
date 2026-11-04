# Tools

asmOS is built and run by the asm-editor web editor. These tools drive it from the command line: the checkout `ASM_EDITOR` points at, else the editor asm-examples is a submodule of, else `asm-editor` beside asm-examples.

## The harness (`harness/`)

`AsmosHarness.test.ts` runs as one of the editor's tests. It compiles asmOS with your computer's GCC, as Compiler Explorer would be asked to, builds it on the editor's x86 Core with this tree as its file system, plays a script of pointer and key actions (`scripts/*.json`), and writes screenshots. Before using it, copy `AsmosHarness.test.ts` into `packages/asm-editor/src/lib/languages/X86/` and `vitest.config.mts` to `packages/asm-editor/.cache/asmos.config.mts` in the editor. Delete the test copy afterwards.

- `asm.sh <tree> <build>` compiles all seven units with GCC into `<build>/proj`. `OS_CFLAGS` adds flags, for example `-DASMOS_COOPERATIVE`, `-DPLATFORM_BLIT=0` or `-DPLATFORM_FILE_SYSTEM=0`.
- `play.sh <build> <script> <shots>` plays `scripts/<script>.json`. `AUTOSTART="app arg;app arg"` starts applications first.
- `all.sh <build> <shots>` plays every script but DOOM's.
- `doom.sh <build> <shots>` plays DOOM's first demo with Frame Times on. Its tic sum must stay `cf6b7f4d` over 5,026 tics.
- `cmp.mjs <shotsA> <shotsB>` compares screenshots pixel by pixel, counting the taskbar's clock and CPU meter apart.
- `AsmosBuildOutput.test.ts`, run as the harness is, adds the linked `os/main` and its Build record to an archive from the repository's build (`OS_ARCHIVE`) and checks the result (`OS_CHECK`). The editor's own build of the examples will add it.

Some shots depend on timing: Pac-Man's animation and 1UP blink, the video in the cooperative build, and blinking carets.

### On RISC-V and MIPS

`MarsHarness.test.ts` does the same for the RISC-V and MIPS ports on the editor's RARS and MARS. It takes the port `build.mjs` packs, compiles its seven units as Compiler Explorer would be asked to, but with a local Clang 21.1.0 (`localClang.ts`; point `ASMOS_CLANG` at the `clang` of LLVM's 21.1.0 release, the version Compiler Explorer runs), builds the Program with its Compilation records so the units link as in the editor, and plays a script on the Screen's Keyboard and Mouse, which reach asmOS through the Input controller. Copy `MarsHarness.test.ts` and `localClang.ts` into `packages/asm-editor/src/lib/languages/mars/` and `vitest.config.mts` to `packages/asm-editor/.cache/asmos.config.mts` in the editor, and delete the copies afterwards.

- `sim.sh <RISC-V|MIPS> <script> <shots>` compiles the units into `BUILD` (default `/tmp/asmos-<target>`) and plays `scripts/<script>.json`. `KEEP=os/main.c,os/apps/apps.c,...` takes the units it lists, of the seven, as compiled last time instead, `USE_ASM=1` the tree's own Generated assembly, `AUTOSTART` starts applications.
- `sim-all.sh <RISC-V|MIPS> <shots>` compiles once and plays every `sim-` script but DOOM's. Their coordinates are for the desktop x86 has too, DOOM's icon on it. It compiles without source annotations (`OS_ANNOTATIONS=0`): with them DOOM's Generated assembly takes the Project over the 16 MiB the editor's file system holds. `sim-busy.json` times the Start menu, Notepad and a drag while the Terminal sorts 30,000 lines, a loop that never gives the processor up, which only the timer's preemption keeps from freezing the desktop. `sim-stack-guard.json` opens Notepad, writes into the guard at the bottom of its stack slot as a stack overrunning it would, and checks that asmOS ends Notepad with a message and opens it again. `sim-low-memory.json` is for a build short of memory, `ASMOS_CFLAGS=-DASMOS_HEAP_END=0x10600000 AUTOSTART="video videos/bad-apple.vid"`, about 1.5 MB left once asmOS has started: the Video Player cannot read its 2.4 MB video and says so, and asmOS goes on.
- `AUTOSTART=doom sim.sh <RISC-V|MIPS> sim-doom <shots>` plays DOOM: the title, the Game menu, the first attract demo to its end, DOOM's menu, a new game, and DOOM opened again, which says to restart asmOS. It prints the demo's tics, their sum, which must be x86's `cf6b7f4d` over 5,026 tics, and its frames a second in tenths (`doom_last_tenths`). The demo takes three and a half minutes (RISC-V's WebAssembly build) to nine (MIPS's JavaScript build). Give it `OS_LEAVE_OUT=videos/`: with DOOM's Generated assembly the Project is over the 16 MiB the editor's file system holds.
- A `stats` line also gives the memory: static data (the screen's 1.2 MB in it), the heap and what is left of the 64 MB. `mark` and `untilPixel` time how long until the Screen shows a colour at a point, as `sim-timing.json` times the start, the Start menu, a window opening and a drag. `audio` counts what the program played since the last one, MIDI messages and notes on the MIDI port and frames on the sampled-audio output, and `stacks` how deep each process's stack slot has been written above its 256-byte guard (`{"stacks": "label", "slot": 65536, "slots": 32}`, the slot size and count of `os/platform/sim/machine.h`). `poke` writes a word between two slices (`{"poke": [address, value]}`).
- `peek` prints the words at labels of the program, a C program's static variables among them (`{"peek": ["doom_last_sum"]}`), and `untilPeek` waits until one is at least a value (`{"untilPeek": ["doom_last_number", 1]}`).
- `OS_LEAVE_OUT=videos/,...` leaves Files out of the run's file system, though the Build still has them; `OS_ANNOTATIONS=0` compiles without source annotations, as the editor's `recompile.task.ts` does, where the editor's Compile has them by default.
- `OS_WASM=1` runs the Core's WebAssembly build, which the editor runs in a browser, rather than its JavaScript build, and `OS_UNDO` sets the Undo history's size (0, the Programs' setting, by default). The compiler reports the size of each answer it stands in for, and how large Compiler Explorer's would be.

## The Assembly Editor's tests (`ide-tests/`)

`run.sh` builds `os/ide/rvcli.c` for your computer and runs every program in `corpus/` and `examples/` through it and through the editor's RISC-V Core, comparing the machine code, the data, the output, how each one ended and the registers. `SAN=1 run.sh` does the same under AddressSanitizer and UndefinedBehaviorSanitizer.
