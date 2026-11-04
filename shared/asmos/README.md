# asmOS

asmOS opens a desktop on the Screen, with a Start menu, windows and eighteen applications, including Pac-Man, DOOM and an editor for RISC-V assembly. It is written in C, with a little assembly for x86, RISC-V and MIPS.

## Running it

1. Select the Program, `os/main.c.asm` on x86, `os/main.c.riscv` on RISC-V or `os/main.c.mips` on MIPS, and press **Build**, then **Run**.
2. Click the Screen, so that it gets the keyboard and the mouse. **Shift+Tab** gives the keyboard back to the page.
3. Double-click an icon, open the **Start** menu, or right-click the desktop. **Shut Down** ends the program.

The generated assembly is already included. You can Build and Run without compiling the C source first.

All three targets include the applications, with two differences to know before playing: the MIPS project omits `videos/`, so the Video Player has no bundled Bad Apple!! video; on RISC-V and MIPS, restart asmOS before reopening DOOM. See [On RISC-V and MIPS](#on-risc-v-and-mips) for the other platform differences.

The Screen is 640 × 480. On x86 the `@screen` comment at the top of `os/main.c` sets its size; on RISC-V and MIPS, `SIM_SCREEN` in `os/platform/sim/screen.c` does. To start applications automatically, list them in `autostart.txt`, one command per line, such as `notepad welcome.txt`.

## The applications

Every application is a process of its own: open two Notepads and both keep running, and Task Manager can end any of them.

| Application | What it does |
| ----------- | ------------ |
| Notepad | Edits text files, with selection, the clipboard, Open and Save As. |
| Calculator | The four operations, percent and sign, with the mouse or the keys. |
| Paint | Pencil, brush, eraser, fill, line, airbrush, rectangle, ellipse and a movable selection; flips, invert, canvas size and five levels of Undo. Saves `.bmp` files. |
| Clock | An analog clock that scales with its window, a digital view and a stopwatch with laps, in UTC. |
| Image Viewer | Shows `.bmp` pictures: fit, actual size, zoom, the previous and next picture of the folder. |
| Music Player | Plays the Standard MIDI files in `music` on the General MIDI synthesizer. |
| Video Player | Plays black-and-white `.vid` videos with sound. `videos/bad-apple.vid` is included on x86 and RISC-V. See below. |
| Hex Viewer | A file's bytes in hex and as characters, with Go To. |
| Assembly Editor | Writes, builds, runs and debugs RISC-V programs, with breakpoints, Step back, the registers, the memory, a console and a bitmap display: see below. |
| Pac-Man | The arcade game, ported from the RISC-V example, its sound mixed in C and played through `/dev/dsp`, or the sampled-audio output on RISC-V and MIPS. |
| DOOM | id Software's DOOM of 1993, through doomgeneric, playing the IWAD in `games/doom`: see below. |
| Blocks | A falling-blocks game with Korobeiniki on MIDI. |
| Minesweeper | Three levels, chording and best times. |
| Terminal | A shell of about forty commands: `ls`, `cd`, `mkdir`, `cat`, `grep`, `find`, `tree`, `cp`, `mv`, `rm`, `ps`, `kill`, `calc`, `open` and more, with `>` into a file and `\|` from one command to the next; `help` lists them. |
| Files | Browses the project's folders and files: open, open with, rename, delete, copy and paste, and drag a file onto the desktop or another application. |
| Task Manager | The processes with their processor and memory, End Process, and a processor graph. |
| Settings | The wallpaper, the colour theme, the double-click speed, the clock's format and sounds, saved in `asmos.ini`. |
| About asmOS | The system and what it is doing now. |

The desktop shows the files in the project's `Desktop` folder beside the applications. Drop a file on the desktop to copy it there. The project keeps a folder only while a file is in it, so asmOS puts a hidden `.keep` file in each folder it makes.

## DOOM

Open DOOM from the desktop or Start menu. It uses [doomgeneric](https://github.com/ozkl/doomgeneric), the portable Chocolate Doom, to run in an asmOS window.

- **The IWAD.** DOOM plays the first of `doom1.wad`, `doom.wad`, `doom2.wad`, `plutonia.wad`, `tnt.wad`, `freedoom1.wad` and `freedoom2.wad` it finds in `games/doom/`, or a `.wad` file opened with it. `doom1.wad` is the shareware episode.
- **Keys.** W and S or the up and down arrows move; left and right turn. A and D, `,` and `.`, or Alt with the arrows strafe. F, the left mouse button or Ctrl fires. E or Space opens doors, Shift runs, 1 to 7 choose a weapon, Tab shows the map and Esc opens the menu. W, A, S, D, E and F work as letters in DOOM's menu, including saved-game names and cheats. In the browser Ctrl+W closes the tab, so use F or the mouse to fire while moving with W. Help > Keys lists the controls.
- **Sound.** Effects use sampled audio and music uses the General MIDI synthesizer. While another application, such as Pac-Man, has the sampled-audio output, DOOM plays without effects. Change the sound and music volume in DOOM's Options.
- **Saved games and settings** go in `games/doom/savegames/` and `games/doom/default.cfg`.
- **Speed.** Expect a few frames per second. This project has **Undo enabled** turned off in the program's Settings, because recording every instruction slows DOOM down. A smaller view (`-`) or Low detail in DOOM's Options reduces the drawing work. Game > FPS Counter toggles the frame-rate display. On RISC-V and MIPS the game world can also run slower than normal.
- **Another game.** Only one DOOM runs at a time. On x86 you can close it and open it again. On RISC-V and MIPS, restart asmOS before reopening it.

The engine is in `os/doom/doomgeneric/`. The platform code in `os/doom/i_asmos.c` provides the window, keyboard messages and clock, and reads the whole IWAD into memory. `os/doom/doomlibc.c` implements the C library services DOOM needs, including allocation and files, through asmOS's `os_` calls.

The sound mixer in `os/doom/i_asmos_sound.c` sends effects through `os_pcm_write`. Music comes from the IWAD's MUS lumps. It queues notes a little ahead with `os_midi_note`, so the kernel can play them between DOOM's frames. DOOM also refills the sampled-audio buffer between rendering phases.

Only changed rows of DOOM's picture are converted to window colours and copied again. On x86, **Game > Fast Drawing (AVX2)** is on by default. The assembly loops in `os/doom/doom_simd.asm` draw walls, sprites and floors and convert colours eight pixels at a time. For example, `vpgatherdd` fetches eight texels in one instruction. Turning Fast Drawing off uses DOOM's C loops instead.

To study the drawing cost, **Game > Frame Times** shows milliseconds and thousands of instructions, counted by `os_instructions`. Its labels are L for game logic, T for thinkers, W for walls, F for floors, M for sprites, O for other drawing, bar for the status bar, S for copying into the window and I for messages and sound. The smaller labels break down sight and movement, palette conversion and repeated rows. On x86, **Game > Profile** lists the most frequently sampled functions: the kernel's timer samples them 100 times a second through `os_profile`, using symbols from the build output `os/main`. **Game > Benchmark CPU** measures instruction costs in the emulator. Fast Drawing, Profile and Benchmark CPU are available only on x86.

Reopening DOOM on x86 needs one extra step internally. Its global variables must be restored before another game. Each `os/doom/doom_*.part.asm` wraps a compiled part in labels marking those variables, and `os/doom/asmos_fresh.asm` restores their saved initial values. RISC-V and MIPS have no equivalent reset, which is why those versions require an asmOS restart.

doomgeneric, DOOM's source and asmOS's platform for it are under the GNU General Public License 2, in `os/doom/LICENSE.txt`.

## Assembly Editor

Open the Assembly Editor from the desktop, with `asm FILE` in the Terminal, or by opening a `.riscv` file. File > Examples opens the programs in `code/`: hello, a table of Fibonacci numbers, a keyboard echo, a guessing game, a ball bouncing on the bitmap display, and a speed test.

The editor inside asmOS implements a subset of the web editor's RISC-V target: RV32IM integer assembly with RARS 1.6's syntax, memory map and selected ecalls. It does not support floating-point instructions, CSRs, macros, `.include`, file ecalls or dialogs. A `.riscv` program using those features needs changes to run here. Help > Ecalls and Help > Devices list the available services and devices.

- **Running.** Build (F3) assembles the program and lists errors under Problems; click an error to go to its line. Run (F5) runs, Pause (F9) pauses, Step (F7) runs one instruction and Step back (F8) undoes one, including its printed output. Click the gutter or press F2 for a breakpoint. These are RARS's shortcuts; F10 opens asmOS's menu. Programs run in short slices so the desktop remains usable.
- **What it shows.** Registers changed by the last step appear in red. Memory can start at the data segment, the stack pointer, an address or a label. The console supplies input for read-integer, read-string and read-character ecalls, and for the Keyboard and Display MMIO. A comment such as `# @screen width=256 height=256 unit=4 base=display` configures a bitmap display. Use View to show it beside the registers or in its own window.
- **The machine.** Text starts at 0x00400000, static data at 0x10010000, the heap at 0x10040000 and the stack at 0x7fffeffc. Supported services include printing, reading, sbrk, exit, time, sleep, MIDI notes and random numbers. The random-number generator follows Java's, as RARS's does. Keyboard and Display MMIO starts at 0xffff0000.
- **Speed.** This interpreter runs inside another emulated program, and drawing its window also takes instructions. Run > Keep History for Step Back records each step; turning it off improves speed but disables Step back. The outer program's **Undo enabled** setting also affects speed and is off in this project. `code/speed.riscv` lets you measure the interpreter on your computer.
- **The files.** `os/ide/rv_asm.c` is the assembler and `os/ide/rv_cpu.c` is the interpreter. Both are plain C, independent of asmOS, with their interface in `os/ide/rv.h`. `os/ide/rvcli.c` makes them a command-line program for your own computer. `os/ide/ide.c` provides the window and is compiled separately.
- **File names.** Programs use `.riscv`, the web editor's extension for RISC-V assembly. This matters on x86, where Build treats `.s` and `.asm` files as NASM assembly for asmOS itself.

## Bad Apple!!

On x86 and RISC-V, the Video Player opens `videos/bad-apple.vid` when it starts: 3 minutes 39 seconds of 480 × 360 black-and-white video at 30 frames per second, with its song, in 2.4 MB. The MIPS project omits the video, but includes the player.

Space or a click on the picture pauses it. The bar underneath seeks, Left and Right jump five seconds, and S toggles sound. Resize or maximize the window to scale the picture to fit.

- **The picture.** Each row is stored as the x positions where black changes to white or white to black. Rows are coded against similar reference rows, using the approach of fax compression (ITU-T T.6). A change can often be described as matching the reference, or falling a pixel or two away. The reference can be the row above or a row from the previous frame, shifted up or down to follow movement. An adaptive binary range coder, the one used by LZMA, compresses these decisions further. The picture data averages about 170 bytes per frame.
- **The sound.** ADPCM stores 16,000 samples per second using QOA's layout. A predictor estimates each sample from the four before it. A slice of 20 samples stores the differences from those estimates, using 3 bits per sample and a scale shared by the slice.
- **The clock.** A timer runs every 10 ms to refill `/dev/dsp` on x86, or the sampled-audio output on RISC-V and MIPS. The amount of sound already played determines which video frame to show. Only changed rows are drawn, filling each run of black or white. Other asmOS applications continue running between these updates.
- **Seeking.** A `.vid` has five-second chunks, each starting from a black frame and silence. The player can decode a chunk independently, so seeking lands at the start of a chunk.
- **The files.** `os/apps/video/vid.c` reads the format; its comment describes each byte. `os/apps/video/encode.c` is an encoder for your own computer, separate from asmOS. It makes `.vid` files from frames and sound extracted by ffmpeg; its comment gives the commands. It also reads the output back with `vid.c`, the same decoder the player uses.

Bad Apple!! is a video by Anira to "Bad Apple!! feat. nomico", Alstroemeria Records' arrangement of ZUN's "Bad Apple!!" from the Touhou Project, sung by nomico. They are their authors' own, not under asmOS's license.

## On RISC-V and MIPS

These versions run as one program on the editor's RARS and MARS cores. They share the C application code with x86. `os/platform/sim/` holds their common device support; `os/platform/riscv/` and `os/platform/mips/` hold the processor-specific code.

- **The screen** is the bitmap display, 640 × 480 words of `0x00RRGGBB` in static data, which `SIM_SCREEN` declares (`sim/screen.c`): a word stored there is a pixel on the Screen.
- **Memory.** The editor's MARS and RARS provide 64 MB for static data and the heap, compared with the original simulators' 4 MB. That gives asmOS room for the screen, the compositor's back buffer, pictures, video and DOOM's IWAD and game data. The process table has room for 32 processes, each with a queue of 128 messages. Each process gets a 64 KB stack in the separate 4 MB stack segment (`sim/memory.c`).
- **The keyboard and the mouse** use the editor's Input controller at 0xFFFF0300 (`sim/input.c`). Keys arrive as USB HID usages, which a table translates to asmOS key codes. The controller also provides the pointer position and a queue of button and wheel events with their positions. asmOS sleeps between polls.
- **The timer** switches away from an application after its time slice, through an assembly trap handler (see [Processes](#processes)). RISC-V uses RARS's Timer Tool: `time` counts milliseconds and `timecmp` sets the next interrupt, 10 ms ahead. MIPS uses coprocessor 0's Count and Compare. Count advances once every two instructions, so asmOS calibrates it against syscall 30's clock at startup and prints the result in the Terminal.
- **The clock** is syscall 30 and sleep syscall 32; the count of instructions run, which Task Manager shows, is RISC-V's `instret` counter, and on MIPS the editor's syscall 1101.
- **The files** use the C library's POSIX calls, implemented through MARS's and RARS's file and folder services. `os/platform/files.c` is shared with x86.
- **Sound** uses two devices the editor adds to MARS and RARS (`sim/sound.c`). The MIDI port at 0xFFFF0340 plays Music Player, Blocks and system chimes through the General MIDI synthesizer. The sampled-audio output at 0xFFFF0350 queues the samples produced by Pac-Man, DOOM and the Video Player. These replace x86's MIDI port and `/dev/dsp`; the original MARS and RARS have neither device.
- **Bad Apple!! is not in the MIPS project.** The archive leaves out `videos/` to fit the project size limit alongside DOOM and the generated MIPS assembly. The Video Player is included and starts with nothing to play.
- **DOOM** uses C loops for drawing, colour conversion and sound mixing. Expect a few frames per second and a game world that can move slower than normal. Sound uses the sampled-audio output and music uses the MIDI port. Game > Fast Drawing, Profile and Benchmark CPU are x86-only features, excluded by `os/doom/asmos_x86.h`. After closing DOOM, restart asmOS before opening it again.
- **Speed.** Drawing can be slow: C loops fill pixels and copy them from the back buffer to the screen. These projects have **Undo enabled** off to reduce the cost of recording instructions. Opening and dragging windows may still feel delayed, especially while a game or video is drawing. Timer interrupts let other applications run even when one is busy computing.

## How it works

asmOS implements its own processes, scheduler, memory accounting and window system inside one ordinary program on the emulated machine. Its kernel is part of that program.

The platform layer, `os/platform/`, connects it to the machine. On x86 it uses the Linux framebuffer `/dev/fb0`, input devices under `/dev/input`, MIDI and `/dev/dsp`, clocks, timers and signals. The project's files form its working directory. On RISC-V and MIPS it uses the bitmap display, the editor's Input controller, MIDI and sampled-audio devices, processor timers and interrupts, and syscalls for the clock and project files. The applications share the same C code across all three targets.

### Processes

Each application's `..._main` function runs on its own stack: 256 KB on x86, 64 KB on RISC-V and MIPS. The process has its own message queue and memory allocations. `os_alloc` charges allocations to the process, and asmOS frees them when it ends.

The bottom 256 bytes of each stack form a guard. The kernel fills them when the process starts and checks them whenever it switches away. If a process has overwritten the guard, the kernel ends it with a message. On RISC-V and MIPS, `os_alloc` also returns NULL when the heap has no room for a request, so the application can handle the failure.

Process 0 is the system loop in `main`. It reads the keyboard and pointer, turns input into messages for windows' processes, fires timers and composes the screen.

One process runs at a time. On x86 the kernel switches in a signal handler (`os/platform/x86/switch.c`):

- A process that sleeps or waits for a message or dialog response gives up the processor by sending itself `SIGUSR2`.
- While applications are ready to run, the interval timer sends `SIGALRM` every 10 milliseconds. It takes the processor from a process even if that process never gives it up: **preemption**. This lets the desktop and Task Manager run while an application is stuck in a loop.

Either way, the signal frame contains the interrupted process's general-purpose registers and 512 bytes of x87 and SSE state. The handler saves them, then writes the next process's saved registers into the frame. Returning from the handler resumes that process where it stopped. A new process starts at `process_start` on its own stack.

On RISC-V and MIPS the switch is a trap handler written in assembly, `os/platform/riscv/trap.riscv` and `os/platform/mips/trap.mips`, which the processor enters between two instructions of whatever runs:

- The timer's interrupt, while applications are ready to run: on RISC-V once `time` reaches `timecmp`, which the handler then sets 10 ms on, and on MIPS once Count reaches Compare.
- A process can give the processor up by deliberately trapping. RISC-V requests a user software interrupt with `csrsi uip, 1`, leaving `ecall` for RARS services. MIPS uses a `teq` that always traps, leaving `syscall` for MARS services. The MIPS handler recognizes this yield by its address.
- Any other exception, such as a load from an invalid address, ends asmOS. The Terminal reports the exception, its address and the process.

The handler saves the general-purpose and floating-point registers on the stopped process's stack, along with its return address (`uepc` on RISC-V, EPC on MIPS). It asks the C kernel code in the adjacent `switch.c` which process should run next. It then switches stacks, restores that process's registers and resumes it with `uret` or `eret`. A new process gets a frame in the same layout, with its program counter set to `process_start`.

On RISC-V, the program installs the handler's address in `utvec`. On MIPS, the handler is in `.ktext` at 0x80000180. Build links both trap files through the functions the kernel calls; the MIPS file brings its `.ktext` handler with those functions.

Long loops also contain `os_yield_point` calls for a machine without a timer. On x86, compiling with `-DASMOS_COOPERATIVE` enables this mode: a plain call gives the processor up, saving the registers a called function must preserve (`os/platform/x86/switch.asm`). With timer-based scheduling, these yield points do nothing.

A process can be stopped between instructions, so changes to kernel data need a **kernel lock**. An `os_` call takes the lock, and the timer postpones switching away until the call releases it. Applications use the `os_` calls in `os/os.h` for allocation, output and files rather than calling `malloc`, `printf` or `fopen` directly. They can use pure C library functions that do not change shared state.

### Windows

Every window has its own picture, the client area, which its process draws into. The compositor (`wm.c`) reads that picture without asking the process to draw again.

When something changes, asmOS marks a rectangle of the screen **dirty**. On each pass, the compositor repaints dirty rectangles into a back buffer. It works from the desktop through the window frames and client pictures to the taskbar and menus, then copies the result onto the framebuffer. If a window completely covers a dirty rectangle, only that window needs painting.

The pointer, a dragged file and a resize outline are drawn directly on the framebuffer, over the composed image. Moving a window copies its existing pixels and repaints only the strip it uncovers. A dirty rectangle wholly inside the top window's picture, with no menu over it, can go straight to the framebuffer. DOOM's frames use this shortcut; the back buffer is updated there when something needs to read it.

For a machine short of memory, asmOS can run without a back buffer. Compile with `-DPLATFORM_BACK_BUFFER=0` to try this on x86. Dirty rectangles then go straight to the screen. Removing a pointer or resize outline requires repainting what was underneath. Moving a window copies pixels on the screen itself, after removing the pointer.

A resize gives the window a new picture. The old one is kept until the process takes the `MSG_RESIZE` message, since until then the process may still be drawing into it.

### Assembly

`os/platform/x86/blit.asm` fills and copies rectangles of pixels. One `rep` instruction does a whole row, and the emulator runs it as a single instruction: filling the screen takes a few thousand instructions, where a C loop storing one pixel at a time takes over 700,000. Of the `rep` instructions `rep movsb` runs fastest by far, so rows are copied by bytes, and a fill stores its first row with `rep stosd` and copies it into the others. RISC-V and MIPS have no such instructions, and their pixel loops are C, `os/blit.c`: there the assembly is the trap handler.

### Files

The code is in `os/`, in four groups compiled separately: the system in `os/` itself, applications in `os/apps/`, the Assembly Editor in `os/ide/` and DOOM in `os/doom/`. Everything else is asmOS's disk: `Desktop`, `code`, `games`, `music`, `pictures`, `videos` where included, `welcome.txt`, and the `asmos.ini` file Settings writes.

| File | Part |
| ---- | ---- |
| `os/os.h` | The system interface: everything the applications may call |
| `os/main.c` | The kernel's half: start-up, the system process's loop, shut down. Includes the files below |
| `os/kernel.c` | Processes, the scheduler, memory, files, messages, timers, the clipboard, settings, sampled sound |
| `os/wm.c` | The window manager: windows, menus, the desktop, the taskbar, drag and drop, the pointer |
| `os/dialogs.c` | The message box, the input box, the Open and Save As dialog, the Run box |
| `os/gfx.c` | Drawing into a picture, clipped: rectangles, lines, circles, bevels, text |
| `os/input.c` | The keyboard and the pointer, as the platform reads them: characters, the keys held, double clicks |
| `os/sound.c` | Notes for the General MIDI synthesizer, on the platform's MIDI port |
| `os/platform/platform.h`, `platform.c`, `files.c` | The platform interface, selection of machine-specific code and POSIX file access |
| `os/platform/x86/` | x86: Linux's framebuffer, input devices, clocks, sound and signals, and `blit.asm` |
| `os/platform/sim/` | What RISC-V and MIPS share on RARS and MARS: the bitmap display, the Input controller, the clock, the stacks, the MIDI port and the sampled-audio output |
| `os/platform/riscv/`, `os/platform/mips/` | Each processor's trap handler, in assembly (`trap.riscv`, `trap.mips`), its C half with the timer (`switch.c`), and its instruction counter |
| `os/bmp.c` | Reading and writing BMP pictures |
| `os/font.c`, `os/icons.c` | The 8 × 13 font (X11's public-domain "fixed") and the icons |
| `os/apps/apps.c` | The applications' half: the application table, and the applications beside it in `os/apps/` |
| `os/apps/video.c`, `os/apps/video/vid.c` | The Video Player, and the `.vid` format it plays |
| `os/apps/video/encode.c` | The `.vid` encoder: a program for your own computer, not compiled with asmOS |
| `os/ide/ide.c` | The Assembly Editor's window, which includes the two files below |
| `os/ide/rv.h`, `os/ide/rv_asm.c`, `os/ide/rv_cpu.c` | The RISC-V assembler and interpreter, plain C without asmOS |
| `os/ide/rvcli.c` | A command-line wrapper for the assembler and interpreter, separate from asmOS |
| `os/doom/doom_*.c`, `os/doom/doom_*.part.asm` | DOOM's four parts, with x86 assembly wrappers marking their variables |
| `os/doom/` | DOOM's asmOS platform: `i_asmos.c`, `i_asmos_sound.c`, `doomlibc.c`, x86 assembly loops and the other `asmos_` files. `asmos_x86.h` selects the x86-only features |
| `os/doom/doomgeneric/` | The engine, with each local change marked by an `asmOS:` comment |

## Compiling it again

You only need Compile after changing the C source. Select the source file that includes your change, then press **Compile** before Build:

| Changed code | Source to compile |
| ------------ | ----------------- |
| System code in `os/` | `os/main.c` |
| Applications in `os/apps/` | `os/apps/apps.c` |
| Assembly Editor in `os/ide/` | `os/ide/ide.c` |
| DOOM code in `os/doom/` | The `doom_*.c` file that includes it |

There are seven compilation units. DOOM is split into `os/doom/doom_game.c`, `doom_play.c`, `doom_render.c` and `doom_world.c` to stay within Compiler Explorer's 1 MiB source limit per compilation. Build links the compiled parts using the names declared in `os/os.h` and DOOM's headers.

On x86, `os/main.c` and `os/apps/apps.c` use **-O1**. The Assembly Editor and DOOM use **-O2**. The generated files have the `.c.asm` suffix; `os/main.c.asm` is the program's entry. Each DOOM part is assembled through its `doom_*.part.asm` wrapper.

On RISC-V and MIPS, Clang compiles the same seven units to `.c.riscv` or `.c.mips` files. `os/main.c` and `os/ide/ide.c` use **-O2**; `os/apps/apps.c` and DOOM's four parts use **-Os** to keep the generated assembly smaller. Build links them directly, without the x86 `.part.asm` wrappers, along with the processor's assembly trap handler.

Before recompiling on RISC-V or MIPS, turn **Include source annotations when compiling** off in the editor's Settings. The bundled generated assembly omits them to stay within the project size limit.
