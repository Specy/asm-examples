# Tools

These tools build and run asmOS from the command line, with the asm-editor web editor's Cores. They use the editor checkout that `ASM_EDITOR` points to. If `ASM_EDITOR` is not set, they use the editor that asm-examples is a submodule of, or else an `asm-editor` folder next to asm-examples.

## The harness (`harness/`)

`AsmosHarness.test.ts` runs as one of the editor's tests. It compiles asmOS with your computer's GCC, using the same options Compiler Explorer would get, builds it on the editor's x86 Core with this folder as its file system, plays a script of pointer and key actions (`scripts/*.json`), and saves screenshots.

To set it up, copy `AsmosHarness.test.ts` into the editor's `packages/asm-editor/src/lib/languages/X86/`, and `vitest.config.mts` to `packages/asm-editor/.cache/asmos.config.mts`. Delete the copied test when you are done.

- `asm.sh <tree> <build>` compiles all seven units with GCC into `<build>/proj`. Add flags with `OS_CFLAGS`, for example `-DASMOS_COOPERATIVE`, `-DPLATFORM_BLIT=0` or `-DPLATFORM_FILE_SYSTEM=0`.
- `play.sh <build> <script> <shots>` plays `scripts/<script>.json`. To start applications first, set `AUTOSTART="app arg;app arg"`.
- `all.sh <build> <shots>` plays every script except DOOM's.
- `doom.sh <build> <shots>` plays DOOM's first demo with Frame Times on. The sum of its tics must stay `cf6b7f4d` over 5,026 tics.
- `cmp.mjs <shotsA> <shotsB>` compares two sets of screenshots pixel by pixel. It counts differences in the taskbar's clock and CPU meter separately.
- `AsmosBuildOutput.test.ts` runs the same way as the harness. It adds the linked `os/main` and its Build record to an archive built by the repository's build (`OS_ARCHIVE`), and checks the result (`OS_CHECK`). Later, the editor's own build of the examples will do this.

Some screenshots depend on timing: Pac-Man's animation and blinking 1UP, the video in the cooperative build, and blinking text cursors.

### On RISC-V and MIPS

`MarsHarness.test.ts` does the same for the RISC-V and MIPS versions, on the editor's RARS and MARS. It takes the files that `build.mjs` packs, and compiles the seven units with the options Compiler Explorer would get, but with a local Clang 21.1.0 (`localClang.ts`). Set `ASMOS_CLANG` to the `clang` of LLVM's 21.1.0 release, the version Compiler Explorer runs. The harness builds the Program with its Compilation records, so the units link as they do in the editor, and plays a script on the Screen's Keyboard and Mouse, which reach asmOS through the Input controller.

To set it up, copy `MarsHarness.test.ts` and `localClang.ts` into the editor's `packages/asm-editor/src/lib/languages/mars/`, and `vitest.config.mts` to `packages/asm-editor/.cache/asmos.config.mts`. Delete the copies when you are done.

- `sim.sh <RISC-V|MIPS> <script> <shots>` compiles the units into `BUILD` (by default `/tmp/asmos-<target>`) and plays `scripts/<script>.json`. These variables change what it does:
  - `KEEP=os/main.c,os/apps/apps.c,...` reuses the last compiled version of the units it lists.
  - `USE_ASM=1` uses the Generated assembly already in the tree.
  - `AUTOSTART` starts applications first.
- `sim-all.sh <RISC-V|MIPS> <shots>` compiles once, without source annotations (`OS_ANNOTATIONS=0`), and plays every `sim-` script except DOOM's. Their coordinates match the x86 desktop, including DOOM's icon. Some scripts test specific behaviour:
  - `sim-busy.json` times the Start menu, Notepad and a drag while the Terminal sorts 30,000 lines. The sort never gives up the processor, so only the timer's preemption keeps the desktop responsive.
  - `sim-stack-guard.json` opens Notepad and writes into the guard at the bottom of its stack, as a stack overflow would. It checks that asmOS ends Notepad with a message, and that Notepad opens again.
  - `sim-low-memory.json` needs a build with little memory: `ASMOS_CFLAGS=-DASMOS_HEAP_END=0x10600000 AUTOSTART="video videos/bad-apple.vid"`, which leaves about 1.5 MB once asmOS has started. The Video Player cannot load its 2.4 MB video and says so, and asmOS keeps running.
- `AUTOSTART=doom sim.sh <RISC-V|MIPS> sim-doom <shots>` plays DOOM: the title, the Game menu, the first demo to its end, DOOM's menu, a new game, and opening DOOM again, which asks you to restart asmOS. It prints the demo's tics and their sum, which must match x86's `cf6b7f4d` over 5,026 tics, and the frame rate in tenths of a frame per second (`doom_last_tenths`). The demo takes from three and a half minutes (RISC-V's WebAssembly build) to nine minutes (MIPS's JavaScript build).

A script can use these actions:

- `stats` prints the memory use: static data (including the screen's 1.2 MB), the heap, and what is left of the 64 MB.
- `mark` and `untilPixel` measure how long it takes until the Screen shows a colour at a point. `sim-timing.json` uses them to time startup, the Start menu, opening a window and a drag.
- `audio` counts what the program played since the last `audio`: MIDI messages and notes on the MIDI port, and frames on the sampled-audio output.
- `stacks` reports how much of each process's stack has been used above its 256-byte guard: `{"stacks": "label", "slot": 65536, "slots": 32}`, with the stack size and count from `os/platform/sim/machine.h`.
- `poke` writes a word between two slices: `{"poke": [address, value]}`.
- `peek` prints the words at labels of the program, including a C program's static variables: `{"peek": ["doom_last_sum"]}`.
- `untilPeek` waits until a label's word reaches at least a value: `{"untilPeek": ["doom_last_number", 1]}`.

These variables change how a run is set up:

- `OS_LEAVE_OUT=videos/,...` leaves Files out of the run's file system. The Build still has them.
- `OS_ANNOTATIONS=0` compiles without source annotations, as the editor's `recompile.task.ts` does. The editor's Compile adds them by default.
- `OS_WASM=1` runs the Core's WebAssembly build, which is what the editor runs in a browser, instead of its JavaScript build.
- `OS_UNDO` sets the size of the Undo history. It is 0 by default, the same as the Programs' setting.

The local compiler reports the size of each answer it gives, and how large Compiler Explorer's answer would be.

## The Assembly Editor's tests (`ide-tests/`)

`run.sh` builds `os/ide/rvcli.c` for your computer, then runs every program in `corpus/` and `examples/` both through it and through the editor's RISC-V Core. It compares the machine code, the data, the output, how each program ended, and the registers. `SAN=1 run.sh` does the same under AddressSanitizer and UndefinedBehaviorSanitizer.
