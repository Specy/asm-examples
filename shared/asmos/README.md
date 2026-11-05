# asmOS

asmOS is a desktop operating system that runs as one program on the emulated machine. It is written in C, with a little assembly, for x86, RISC-V and MIPS. It opens a desktop on the Screen with a Start menu, windows and eighteen applications, including Pac-Man, DOOM and an editor for RISC-V assembly.

## Running it

1. Pick the Program: `os/main.c.asm` on x86, `os/main.c.riscv` on RISC-V or `os/main.c.mips` on MIPS.
2. Click the Screen, so it gets the keyboard and the mouse. **Shift+Tab** gives the keyboard back to the page.
3. Double-click an icon, open the **Start** menu, or right-click the desktop. **Shut Down** ends the program.

The C source is already compiled, so you don't need to press Compile unless you change it. See [Compiling it again](#compiling-it-again).

The Screen is 640 × 480. On x86, the `@screen` comment at the top of `os/main.c` sets its size. On RISC-V and MIPS, `SIM_SCREEN` in `os/platform/sim/screen.c` sets it. To start applications automatically, list them in `autostart.txt`, one command per line, such as `notepad welcome.txt`.

On RISC-V and MIPS, restart asmOS before you open DOOM a second time. [On RISC-V and MIPS](#on-risc-v-and-mips) lists the other differences.

## The applications

Each application runs as its own process. If you open two Notepads, both keep running, and Task Manager can end either of them.

| Application | What it does |
| ----------- | ------------ |
| Notepad | Edits text files, with selection, the clipboard, Open and Save As. |
| Calculator | The four operations, percent and sign, with the mouse or the keyboard. |
| Paint | Pencil, brush, eraser, fill, line, airbrush, rectangle, ellipse and a movable selection; flips, invert, canvas size and five levels of Undo. Saves `.bmp` files. |
| Clock | An analog clock that scales with its window, a digital view and a stopwatch with laps, in UTC. |
| Image Viewer | Shows `.bmp` pictures: fit, actual size, zoom, and the previous and next picture in the folder. |
| Music Player | Plays the Standard MIDI files in `music` on the General MIDI synthesizer. |
| Video Player | Plays black-and-white `.vid` videos with sound. Every target includes `videos/bad-apple.vid`. See [Bad Apple!!](#bad-apple). |
| Hex Viewer | Shows a file's bytes in hex and as characters, with Go To. |
| Assembly Editor | Writes, builds, runs and debugs RISC-V programs, with breakpoints, Step back, the registers, the memory, a console and a bitmap display. See [Assembly Editor](#assembly-editor). |
| Pac-Man | The arcade game, ported from the RISC-V example. Its sound is mixed in C and played through `/dev/dsp` on x86, or the sampled-audio output on RISC-V and MIPS. |
| DOOM | id Software's DOOM of 1993, through doomgeneric, playing the IWAD in `games/doom`. See [DOOM](#doom). |
| Blocks | A falling-blocks game, with Korobeiniki on MIDI. |
| Minesweeper | Three levels, chording and best times. |
| Terminal | A shell with about forty commands, such as `ls`, `cd`, `mkdir`, `cat`, `grep`, `find`, `tree`, `cp`, `mv`, `rm`, `ps`, `kill`, `calc` and `open`. `>` writes to a file and `\|` pipes one command into the next. `help` lists the commands. |
| Files | Browses the project's folders and files: open, open with, rename, delete, copy and paste, and drag a file onto the desktop or into another application. |
| Task Manager | Lists the processes with their processor and memory use, ends a process, and graphs the processor's load. |
| Settings | The wallpaper, the colour theme, the double-click speed, the clock's format and the sounds, saved in `asmos.ini`. |
| About asmOS | Shows the system and what it is doing now. |

The desktop shows the files in the project's `Desktop` folder next to the applications. Drop a file on the desktop to copy it there. The project only keeps folders that contain a file, so asmOS puts a hidden `.keep` file in each folder it creates.

## DOOM

DOOM runs in an asmOS window through [doomgeneric](https://github.com/ozkl/doomgeneric), a portable version of Chocolate Doom. Open it from the desktop or the Start menu.

- **The IWAD.** DOOM plays the first IWAD it finds in `games/doom/`, in this order: `doom1.wad`, `doom.wad`, `doom2.wad`, `plutonia.wad`, `tnt.wad`, `freedoom1.wad`, `freedoom2.wad`. You can also open a `.wad` file with DOOM. `doom1.wad` is the shareware episode.
- **Keys.** W and S, or the up and down arrows, move. Left and right turn. A and D, `,` and `.`, or Alt with the arrows strafe. F, the left mouse button or Ctrl fires. E or Space opens doors, Shift runs, 1 to 7 choose a weapon, Tab shows the map and Esc opens the menu. In DOOM's menu, W, A, S, D, E and F type letters, for saved-game names and cheats. In the browser, Ctrl+W closes the tab, so fire with F or the mouse while you move with W. Help > Keys lists the controls.
- **Sound.** Effects use sampled audio, and music uses the General MIDI synthesizer. While another application, such as Pac-Man, is using the sampled-audio output, DOOM plays without effects. Set the sound and music volume in DOOM's Options.
- **Saved games and settings** are stored in `games/doom/savegames/` and `games/doom/default.cfg`.
- **Speed.** Expect a few frames per second. **Undo enabled** is off in this Program's Settings, because recording every instruction slows DOOM down. A smaller view (`-`) or Low detail in DOOM's Options makes drawing faster. Game > FPS Counter shows the frame rate. On RISC-V and MIPS, the game world can also run slower than normal.
- **Opening it again.** Only one DOOM runs at a time. On x86, you can close it and open it again. On RISC-V and MIPS, restart asmOS before you open it again.

The engine is in `os/doom/doomgeneric/`. `os/doom/i_asmos.c` is the platform code: it provides the window, the keyboard messages and the clock, and reads the whole IWAD into memory. `os/doom/doomlibc.c` implements the C library functions DOOM needs, such as allocation and files, with asmOS's `os_` calls.

The sound mixer in `os/doom/i_asmos_sound.c` sends effects through `os_pcm_write`. Music comes from the IWAD's MUS lumps: the mixer queues notes a little ahead with `os_midi_note`, so the kernel can play them between DOOM's frames. DOOM also refills the sampled-audio buffer between rendering phases.

asmOS converts and copies only the rows of DOOM's picture that changed. On x86, **Game > Fast Drawing (AVX2)** is on by default: the assembly loops in `os/doom/doom_simd.asm` draw walls, sprites and floors, and convert colours, eight pixels at a time. For example, `vpgatherdd` fetches eight texels in one instruction. With Fast Drawing off, DOOM uses its C loops instead.

To see where the time goes, open **Game > Frame Times**. It shows milliseconds and thousands of instructions, counted by `os_instructions`, for each part of a frame: L for game logic, T for thinkers, W for walls, F for floors, M for sprites, O for other drawing, bar for the status bar, S for copying into the window, and I for messages and sound. The smaller labels split up sight and movement, palette conversion and repeated rows. On x86, **Game > Profile** lists the functions that were sampled most often: the kernel's timer samples them 100 times a second through `os_profile`, using the symbols of the build output `os/main`. **Game > Benchmark CPU** measures how much each instruction costs in the emulator. Fast Drawing, Profile and Benchmark CPU are x86 only.

To open DOOM again on x86, asmOS first restores DOOM's global variables to their initial values. Each `os/doom/doom_*.part.asm` wraps a compiled part of DOOM in labels that mark its variables, and `os/doom/asmos_fresh.asm` restores their saved initial values. RISC-V and MIPS have no such reset, which is why you need to restart asmOS there.

doomgeneric, DOOM's source and asmOS's platform code for it are under the GNU General Public License 2, in `os/doom/LICENSE.txt`.

## Assembly Editor

Open the Assembly Editor from the desktop, with `asm FILE` in the Terminal, or by opening a `.riscv` file. File > Examples opens the programs in `code/`: hello, a table of Fibonacci numbers, a keyboard echo, a guessing game, a ball bouncing on the bitmap display, and a speed test.

The Assembly Editor supports a subset of the web editor's RISC-V target: RV32IM integer assembly, with RARS 1.6's syntax, memory map and some of its ecalls. It does not support floating-point instructions, CSRs, macros, `.include`, file ecalls or dialogs, so a `.riscv` program that uses them needs changes to run here. Help > Ecalls and Help > Devices list the services and devices it supports.

- **Running.** Build (F3) assembles the program and lists the errors under Problems; click an error to go to its line. Run (F5) runs, Pause (F9) pauses, Step (F7) runs one instruction and Step back (F8) undoes one, including what it printed. Click the gutter or press F2 to set a breakpoint. These are RARS's shortcuts; F10 opens asmOS's menu. Programs run in short slices, so the desktop stays usable.
- **What it shows.** Registers changed by the last step are red. The memory view can start at the data segment, the stack pointer, an address or a label. The console provides input for the read-integer, read-string and read-character ecalls, and for the Keyboard and Display MMIO. A comment such as `# @screen width=256 height=256 unit=4 base=display` sets up a bitmap display. Use View to show it next to the registers or in its own window.
- **The machine.** Text starts at 0x00400000, static data at 0x10010000, the heap at 0x10040000 and the stack at 0x7fffeffc. The services include printing, reading, sbrk, exit, time, sleep, MIDI notes and random numbers. The random-number generator is Java's, as in RARS. Keyboard and Display MMIO starts at 0xffff0000.
- **Speed.** The Assembly Editor's interpreter runs inside another emulated program, and drawing its window also takes instructions. Run > Keep History for Step Back records every step; turn it off to run faster, without Step back. The outer program's **Undo enabled** setting also slows it down, and it is off in this project. Run `code/speed.riscv` to measure the interpreter on your computer.
- **The files.** `os/ide/rv_asm.c` is the assembler and `os/ide/rv_cpu.c` is the interpreter. Both are plain C that does not depend on asmOS, with their interface in `os/ide/rv.h`. `os/ide/rvcli.c` turns them into a command-line program for your own computer. `os/ide/ide.c` is the window, and is compiled separately.
- **File names.** Programs use `.riscv`, the web editor's extension for RISC-V assembly, because on x86, Build treats `.s` and `.asm` files as NASM assembly that belongs to asmOS itself.

## Bad Apple!!

The Video Player opens `videos/bad-apple.vid` when it starts: 3 minutes 39 seconds of 480 × 360 black-and-white video at 30 frames per second, with its song, in 2.4 MB.

Press Space or click the picture to pause. Drag the bar underneath to seek, press Left or Right to jump five seconds, and press S to turn the sound on or off. Resize or maximize the window to scale the picture.

- **The picture.** Each row is stored as the x positions where the colour changes between black and white. Each row is coded against a similar reference row, as fax compression (ITU-T T.6) does: a change is often the same as in the reference, or a pixel or two away. The reference is the row above, or a row of the previous frame, shifted up or down to follow movement. An adaptive binary range coder, the one LZMA uses, compresses these decisions further. The picture takes about 170 bytes per frame.
- **The sound.** The sound is ADPCM at 16,000 samples per second, in QOA's layout. A predictor estimates each sample from the four before it. A slice of 20 samples stores the differences from those estimates, at 3 bits per sample, with one scale for the whole slice.
- **The clock.** A timer runs every 10 ms to refill `/dev/dsp` on x86, or the sampled-audio output on RISC-V and MIPS. The amount of sound played so far decides which frame to show. Only the rows that changed are drawn, by filling each run of black or white. Other asmOS applications keep running between these updates.
- **Seeking.** A `.vid` file is split into five-second chunks, each starting from a black frame and silence. The player can decode any chunk on its own, so seeking jumps to the start of a chunk.
- **The files.** `os/apps/video/vid.c` reads the format; its comment describes every byte. `os/apps/video/encode.c` is an encoder that runs on your own computer, not in asmOS. It makes `.vid` files from frames and sound extracted with ffmpeg, and its comment gives the commands. It checks its output by reading it back with `vid.c`, the same decoder the player uses.

Bad Apple!! is a video by Anira to "Bad Apple!! feat. nomico", Alstroemeria Records' arrangement of ZUN's "Bad Apple!!" from the Touhou Project, sung by nomico. They belong to their authors and are not under asmOS's license.

## On RISC-V and MIPS

The RISC-V and MIPS versions run as one program on the editor's RARS and MARS Cores. They share the applications' C code with x86. `os/platform/sim/` holds the device code the two have in common, and `os/platform/riscv/` and `os/platform/mips/` hold the code for each processor.

- **The screen** is the bitmap display: 640 × 480 words of `0x00RRGGBB` in static data, declared by `SIM_SCREEN` in `sim/screen.c`. Each word stored there is a pixel on the Screen.
- **Memory.** The editor's MARS and RARS have 64 MB for static data and the heap, instead of the original simulators' 4 MB. That leaves room for the screen, the compositor's back buffer, pictures, video, and DOOM's IWAD and game data. The process table holds up to 32 processes, each with a queue of 128 messages. Each process gets a 64 KB stack in the separate 4 MB stack segment (`sim/memory.c`).
- **The keyboard and the mouse** use the editor's Input controller at 0xFFFF0300 (`sim/input.c`). Keys arrive as USB HID usages, which a table translates into asmOS key codes. The controller also gives the pointer's position and a queue of button and wheel events with their positions. asmOS sleeps between polls.
- **The timer** switches away from an application when its time slice ends, through a trap handler written in assembly (see [Processes](#processes)). RISC-V uses RARS's Timer Tool: `time` counts milliseconds, and `timecmp` sets the next interrupt, 10 ms ahead. MIPS uses coprocessor 0's Count and Compare registers. Count goes up once every two instructions, so asmOS calibrates it against syscall 30's clock at startup, and prints the result in the Terminal.
- **The clock** is syscall 30, and sleeping is syscall 32. The number of instructions run, which Task Manager shows, comes from RISC-V's `instret` counter, and from the editor's syscall 1101 on MIPS.
- **The files** use the C library's POSIX calls, implemented with MARS's and RARS's file and folder services. `os/platform/files.c` is shared with x86.
- **Sound** uses two devices that the editor adds to MARS and RARS (`sim/sound.c`). The MIDI port at 0xFFFF0340 plays Music Player, Blocks and the system sounds on the General MIDI synthesizer. The sampled-audio output at 0xFFFF0350 queues the samples that Pac-Man, DOOM and the Video Player produce. They replace x86's MIDI port and `/dev/dsp`. The original MARS and RARS have neither device.
- **DOOM** uses C loops for drawing, colour conversion and sound mixing. Expect a few frames per second, and a game world that can run slower than normal. Effects use the sampled-audio output and music uses the MIDI port. Game > Fast Drawing, Profile and Benchmark CPU are x86 only, and `os/doom/asmos_x86.h` leaves them out here. After you close DOOM, restart asmOS before you open it again.
- **Speed.** Drawing can be slow, because C loops fill the pixels and copy them from the back buffer to the screen. **Undo enabled** is off in these Programs, so the emulator does not spend time recording instructions. Opening and dragging windows can still lag, especially while a game or a video is drawing. Timer interrupts let other applications run even while one is busy computing.

## How it works

asmOS has its own processes, scheduler, memory accounting and window system, all inside one ordinary program on the emulated machine. Its kernel is part of that program.

The platform layer, `os/platform/`, connects asmOS to the machine. On x86, it uses the Linux framebuffer `/dev/fb0`, the input devices under `/dev/input`, MIDI and `/dev/dsp`, clocks, timers and signals, and the project's files are its working directory. On RISC-V and MIPS, it uses the bitmap display, the editor's Input controller, the MIDI and sampled-audio devices, the processor's timer and interrupts, and syscalls for the clock and the project's files. The applications are the same C code on all three targets.

### Processes

Each application's `..._main` function runs on its own stack: 256 KB on x86, 64 KB on RISC-V and MIPS. Each process has its own message queue and its own memory allocations. `os_alloc` charges each allocation to the process, and asmOS frees them all when the process ends.

The bottom 256 bytes of each stack are a guard. The kernel fills them when the process starts, and checks them each time it switches away. If a process has overwritten the guard, the kernel ends it and shows a message. On RISC-V and MIPS, `os_alloc` also returns NULL when the heap has no room left, so the application can handle the failure.

Process 0 is the system loop in `main`. It reads the keyboard and pointer, turns the input into messages for the windows' processes, fires timers and composes the screen.

One process runs at a time. On x86, the kernel switches processes in a signal handler (`os/platform/x86/switch.c`):

- A process that sleeps, or waits for a message or a dialog's answer, gives up the processor by sending itself `SIGUSR2`.
- While applications are ready to run, the interval timer sends `SIGALRM` every 10 milliseconds. This takes the processor away from a process even if it never gives it up, which is called **preemption**. It keeps the desktop and Task Manager running while an application is stuck in a loop.

Either way, the signal frame holds the interrupted process's general-purpose registers and 512 bytes of x87 and SSE state. The handler saves them, then writes the next process's saved registers into the frame. When the handler returns, that process continues where it stopped. A new process starts at `process_start`, on its own stack.

On RISC-V and MIPS, the switch is a trap handler written in assembly, `os/platform/riscv/trap.riscv` and `os/platform/mips/trap.mips`. The processor enters it between two instructions of whatever is running:

- On a timer interrupt, while applications are ready to run. On RISC-V, the interrupt comes when `time` reaches `timecmp`, and the handler then sets `timecmp` 10 ms ahead. On MIPS, it comes when Count reaches Compare.
- When a process gives up the processor on purpose, by trapping. RISC-V raises a user software interrupt with `csrsi uip, 1`, so `ecall` stays free for RARS's services. MIPS uses a `teq` that always traps, so `syscall` stays free for MARS's services; the MIPS handler recognizes this trap by its address.
- On any other exception, such as a load from an invalid address, asmOS ends. The Terminal reports the exception, its address and the process.

The handler saves the general-purpose and floating-point registers on the stopped process's stack, with its return address (`uepc` on RISC-V, EPC on MIPS). It asks the C kernel code in the `switch.c` next to it which process should run next. Then it switches stacks, restores that process's registers, and resumes it with `uret` or `eret`. A new process gets a frame in the same layout, with its program counter set to `process_start`.

On RISC-V, the program puts the handler's address in `utvec`. On MIPS, the handler is in `.ktext`, at 0x80000180. Build links in each trap file because the kernel calls functions defined in it, and the MIPS file's `.ktext` handler comes along with those functions.

Long loops also call `os_yield_point`, for machines without a timer. On x86, compile with `-DASMOS_COOPERATIVE` to try this mode: each call gives up the processor, saving the registers a function must preserve (`os/platform/x86/switch.asm`). With timer-based scheduling, these calls do nothing.

A process can be stopped between any two instructions, so changes to the kernel's data need a **kernel lock**. Each `os_` call takes the lock, and the timer waits until the call releases it before switching away. For this reason, applications use the `os_` calls in `os/os.h` for allocation, output and files, instead of calling `malloc`, `printf` or `fopen` directly. They can still use C library functions that don't change shared state.

### Windows

Each window has its own picture, the client area, which its process draws into. The compositor (`wm.c`) reads that picture without asking the process to draw again.

When something changes, asmOS marks that rectangle of the screen as **dirty**. On each pass, the compositor repaints the dirty rectangles into a back buffer, from the desktop up through the window frames, the windows' pictures, the taskbar and the menus. Then it copies the result to the framebuffer. If one window covers a dirty rectangle completely, only that window is painted.

The pointer, a dragged file and a resize outline are drawn straight on the framebuffer, on top of the composed picture. Moving a window copies its pixels and repaints only the strip it uncovers. A dirty rectangle that lies entirely inside the top window's picture, with no menu over it, goes straight to the framebuffer. DOOM's frames take this shortcut, and the back buffer is only updated when something needs to read it.

On a machine with little memory, asmOS can run without a back buffer. To try this on x86, compile with `-DPLATFORM_BACK_BUFFER=0`. Dirty rectangles then go straight to the screen. Removing the pointer or a resize outline means repainting what was under it, and moving a window copies pixels on the screen itself, after removing the pointer.

Resizing a window gives it a new picture. asmOS keeps the old one until the process takes the `MSG_RESIZE` message, because until then the process may still be drawing into it.

### Assembly

`os/platform/x86/blit.asm` fills and copies rectangles of pixels. One `rep` instruction handles a whole row, and the emulator counts it as a single instruction: filling the screen takes a few thousand instructions, while a C loop that stores one pixel at a time takes over 700,000. `rep movsb` is by far the fastest `rep` instruction, so rows are copied with `rep movsb`, and a fill stores its first row with `rep stosd`, then copies that row into the others. RISC-V and MIPS have no such instructions, so their pixel loops are in C, in `os/blit.c`, and their only assembly is the trap handler.

### Files

The code is in `os/`, in four groups that are compiled separately: the system in `os/` itself, the applications in `os/apps/`, the Assembly Editor in `os/ide/` and DOOM in `os/doom/`. Everything else is asmOS's disk: `Desktop`, `code`, `games`, `music`, `pictures`, `videos` (where included), `welcome.txt`, and the `asmos.ini` file that Settings writes.

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

You only need Compile after you change the C source. Select the source file that includes your change, then press **Compile** before Build:

| Changed code | Source to compile |
| ------------ | ----------------- |
| System code in `os/` | `os/main.c` |
| Applications in `os/apps/` | `os/apps/apps.c` |
| Assembly Editor in `os/ide/` | `os/ide/ide.c` |
| DOOM code in `os/doom/` | The `doom_*.c` file that includes it |

There are seven compilation units. DOOM is split into `os/doom/doom_game.c`, `doom_play.c`, `doom_render.c` and `doom_world.c`, because Compiler Explorer accepts at most 1 MiB of source per compilation. Build links the compiled parts together through the names declared in `os/os.h` and DOOM's headers.

On x86, `os/main.c` and `os/apps/apps.c` are compiled with **-O1**, and the Assembly Editor and DOOM with **-O2**. The generated files end in `.c.asm`, and `os/main.c.asm` is the program's Entry. Each DOOM part is assembled through its `doom_*.part.asm` wrapper.

On RISC-V and MIPS, Clang compiles the same seven units to `.c.riscv` or `.c.mips` files. `os/main.c` and `os/ide/ide.c` are compiled with **-O2**, and `os/apps/apps.c` and DOOM's four parts with **-Os**, to keep the generated assembly smaller. Build links them directly, without x86's `.part.asm` wrappers, together with the processor's trap handler.
