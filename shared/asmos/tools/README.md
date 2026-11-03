# Tools

asmOS is built and run by the asm-editor web editor, from a checkout of it beside asm-examples (`../asm-editor` from the repository root), or wherever `ASM_EDITOR` points. These tools drive that editor from the command line.

## The harness (`harness/`)

`AsmosHarness.test.ts` runs as one of the editor's tests. It compiles asmOS with your computer's GCC, as Compiler Explorer would be asked to, builds it on the editor's x86 Core with this tree as its file system, plays a script of pointer and key actions (`scripts/*.json`), and writes screenshots. Before using it, copy `AsmosHarness.test.ts` into `packages/asm-editor/src/lib/languages/X86/` and `vitest.config.mts` to `packages/asm-editor/.cache/asmos.config.mts` in the editor. Delete the test copy afterwards.

- `asm.sh <tree> <build>` compiles all seven units with GCC into `<build>/proj`. `OS_CFLAGS` adds flags, for example `-DASMOS_COOPERATIVE`, `-DPLATFORM_BLIT=0` or `-DPLATFORM_FILE_SYSTEM=0`.
- `play.sh <build> <script> <shots>` plays `scripts/<script>.json`. `AUTOSTART="app arg;app arg"` starts applications first.
- `all.sh <build> <shots>` plays every script but DOOM's.
- `doom.sh <build> <shots>` plays DOOM's first demo with Frame Times on. Its tic sum must stay `cf6b7f4d` over 5,026 tics.
- `cmp.mjs <shotsA> <shotsB>` compares screenshots pixel by pixel, counting the taskbar's clock and CPU meter apart.
- `pack.mjs <tree> <out.asmproj> <manifest.json>` packs the tree into an archive, as the editor's examples are packed. `AsmosBuildOutput.test.ts` then adds the linked `os/main` and its Build record (`OS_ARCHIVE`) and checks the result (`OS_CHECK`).

Some shots depend on timing: Pac-Man's animation and 1UP blink, the video in the cooperative build, and blinking carets.

## The Assembly Editor's tests (`ide-tests/`)

`run.sh` builds `os/ide/rvcli.c` for your computer and runs every program in `corpus/` and `examples/` through it and through the editor's RISC-V Core, comparing the machine code, the data, the output, how each one ended and the registers. `SAN=1 run.sh` does the same under AddressSanitizer and UndefinedBehaviorSanitizer.
