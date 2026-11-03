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

`MarsHarness.test.ts` does the same for the RISC-V and MIPS ports on the editor's RARS and MARS. It takes the port `build.mjs` packs, compiles its three units as Compiler Explorer would be asked to, but with a local Clang 21.1.0 (`localClang.ts`; point `ASMOS_CLANG` at the `clang` of LLVM's 21.1.0 release, the version Compiler Explorer runs), builds the Program with its Compilation records so the units link as in the editor, and plays a script on the Screen's Keyboard and Mouse, which reach asmOS through the Input controller. Copy `MarsHarness.test.ts` and `localClang.ts` into `packages/asm-editor/src/lib/languages/mars/` and `vitest.config.mts` to `packages/asm-editor/.cache/asmos.config.mts` in the editor, and delete the copies afterwards.

- `sim.sh <RISC-V|MIPS> <script> <shots>` compiles the units into `BUILD` (default `/tmp/asmos-<target>`) and plays `scripts/<script>.json`. `KEEP=os/main.c,os/apps/apps.c,os/ide/ide.c` takes the units compiled last time instead, `USE_ASM=1` the tree's own Generated assembly, `AUTOSTART` starts applications.
- `sim-all.sh <RISC-V|MIPS> <shots>` compiles once and plays every `sim-` script. Their coordinates are the x86 scripts' moved to a desktop without DOOM and the Video Player.
- A `stats` line also gives the memory: static data (the screen's 1.2 MB in it), the heap and what is left of the 4 MB. `mark` and `untilPixel` time how long until the Screen shows a colour at a point, as `sim-timing.json` times the start, the Start menu, a window opening and a drag.

## The Assembly Editor's tests (`ide-tests/`)

`run.sh` builds `os/ide/rvcli.c` for your computer and runs every program in `corpus/` and `examples/` through it and through the editor's RISC-V Core, comparing the machine code, the data, the output, how each one ended and the registers. `SAN=1 run.sh` does the same under AddressSanitizer and UndefinedBehaviorSanitizer.
