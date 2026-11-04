# asmOS

A small operating system for the Screen of the x86, RISC-V and MIPS Targets: a kernel that runs every application as its own process and switches between them, a compositing window manager with menus, dialogs, drag and drop and a clipboard, a desktop with a Start menu, and eighteen applications, Pac-Man and DOOM among them, a Video Player that plays Bad Apple!!, and an Assembly Editor that runs RISC-V programs. It is written in C, with a little assembly for each machine.

asmOS is not a real kernel: it runs as one ordinary program on the emulated machine. Its processes, scheduler, memory accounting and window system are its own, built on what the machine gives a program. On x86 that is Linux: the framebuffer `/dev/fb0`, the keyboard and the mouse under `/dev/input`, sound on the MIDI port and `/dev/dsp`, timers and signals, and the Project's files as its working directory. On RISC-V and MIPS it is RARS and MARS: a bitmap display in memory, the editor's Input controller for the keyboard and the mouse, its MIDI port and sampled-audio output, the processor's timer and its interrupts, which asmOS's own trap handler takes, and syscalls for the clock and the Project's files. The machine's part is the platform layer, `os/platform/`; the rest is the same C on all three, and the three Projects are packed from one tree.

On RISC-V and MIPS every application is there too, DOOM among them, with a few differences: see [On RISC-V and MIPS](#on-risc-v-and-mips).

## Running it

1. Select the Program, `os/main.c.asm` on x86, `os/main.c.riscv` on RISC-V or `os/main.c.mips` on MIPS, and press **Build**, then **Run**.
2. Click the Screen, so that it gets the keyboard and the mouse. **Shift+Tab** gives the keyboard back to the page.
3. Double-click an icon, open the **Start** menu, or right-click the desktop. **Shut Down** ends the program.

The Screen is 640 × 480, set by the `@screen` comment at the top of `os/main.c` on x86, and on RISC-V and MIPS by the `SIM_SCREEN` of `os/platform/sim/screen.c`. List applications in an `autostart.txt` File, one a line (`notepad welcome.txt`), to have asmOS start them.

## The applications

Every application is a process of its own: open two Notepads and both keep running, and Task Manager can end any of them.

| Application | What it does |
| ----------- | ------------ |
| Notepad | Edits text files, with selection, the clipboard, Open and Save As. |
| Calculator | The four operations, percent and sign, with the mouse or the keys. |
| Paint | Pencil, brush, eraser, fill, line, airbrush, rectangle, ellipse and a selection that moves; flips, invert, canvas size, five levels of Undo; saves `.bmp` Files. |
| Clock | An analog clock that scales with its window, a digital view and a stopwatch with laps, in UTC. |
| Image Viewer | Shows `.bmp` pictures: fit, actual size, zoom, the previous and next picture of the folder. |
| Music Player | Plays the Standard MIDI Files of the `music` folder on the General MIDI synthesizer. |
| Video Player | Plays `.vid` videos, black and white with their sound: `videos/bad-apple.vid`, Bad Apple!!. See below. |
| Hex Viewer | A file's bytes in hex and as characters, with Go To. |
| Assembly Editor | Writes, builds, runs and debugs RISC-V programs, with breakpoints, Step back, the registers, the memory, a console and a bitmap display: see below. |
| Pac-Man | The arcade game, ported from the RISC-V example, its sound mixed in C and played through `/dev/dsp`, or the sampled-audio output on RISC-V and MIPS. |
| DOOM | id Software's DOOM of 1993, through doomgeneric, playing the IWAD in `games/doom`: see below. |
| Blocks | A falling-blocks game with Korobeiniki on MIDI. |
| Minesweeper | Three levels, chording and best times. |
| Terminal | A shell of about forty commands: `ls`, `cd`, `mkdir`, `cat`, `grep`, `find`, `tree`, `cp`, `mv`, `rm`, `ps`, `kill`, `calc`, `open` and more, with `>` into a file and `\|` from one command to the next; `help` lists them. |
| Files | The Project's folders and files: open, open with, rename, delete, copy and paste, and drag a file onto the desktop or another application. |
| Task Manager | The processes with their processor and memory, End Process, and a processor graph. |
| Settings | The wallpaper, the colour theme, the double-click speed, the clock's format and sounds, saved in `asmos.ini`. |
| About asmOS | The system and what it is doing now. |

The desktop shows the files of the Project's `Desktop` folder beside the applications: drop a file on the desktop to copy it there. The Project keeps a folder only while a file is in it, so asmOS puts a hidden `.keep` file in each folder it makes.

## DOOM

DOOM is [doomgeneric](https://github.com/ozkl/doomgeneric), the portable Chocolate Doom, in `os/doom/doomgeneric/`, with its platform written for asmOS beside it in `os/doom/i_asmos.c`: a resizable window, the keys as messages, the clock, and the IWAD read whole into memory. `os/doom/doomlibc.c` gives it the C library it expects, `malloc` and files among it, made of `os_` calls, since a process may not call the C library's own.

- **The IWAD.** DOOM plays the first of `doom1.wad`, `doom.wad`, `doom2.wad`, `plutonia.wad`, `tnt.wad`, `freedoom1.wad` and `freedoom2.wad` it finds in `games/doom/`, or a `.wad` File opened with it. `doom1.wad` is the shareware episode, which id Software lets anyone copy.
- **Keys.** W and S or the up and down arrows move, left and right turn, A and D (or `,` and `.`, or Alt with the arrows) strafe, F, the left button or Ctrl fires, E or Space opens doors, Shift runs, 1 to 7 choose a weapon, Tab shows the map and Esc the menu. W, A, S, D, E and F are letters again in DOOM's menu, so a saved game's name can have them, and cheats work as ever. In the browser Ctrl+W closes the tab, so fire with F or the mouse while moving with W. Help > Keys lists them.
- **Sound.** The effects are mixed in `os/doom/i_asmos_sound.c` and played through `os_pcm_write`, and the music, the IWAD's MUS lumps, is played on the General MIDI synthesizer: its notes are queued with `os_midi_note` a moment ahead, so the kernel plays them in time however slowly DOOM draws. A frame of DOOM lasts longer than the sound the kernel buffers, so DOOM tops it up between the renderer's phases too. While another program, Pac-Man say, has the sampled sound, DOOM plays without effects. Sound and music volume are in DOOM's Options.
- **Saved games and settings** go in `games/doom/savegames/` and `games/doom/default.cfg`.
- **Speed.** The emulator runs a few million instructions a second and a frame of DOOM takes about two million, so DOOM shows a few frames a second. Turn **Undo enabled** off in the Program's Settings, which this Project does: keeping the history of every instruction makes DOOM three times slower. The frames a second show in the picture's top right corner (Game > FPS Counter hides them), and Game > Frame Times shows over the picture where a frame goes, in milliseconds and in thousands of instructions (`os_instructions`, the emulator's own count): L the game's logic, T its thinkers and within them sight and move, W walls, F floors, M sprites, O DOOM's other drawing and bar its status bar, S the copy into the window and within it pal the palette and dbl the repeated rows, I messages and sound; and how many millions of instructions a second DOOM, the system and all ran, and a sum of the last attract demo's tics, which must not change from one version of DOOM to the next. Only the rows of DOOM's picture that changed since the last frame are turned into colours and shown again, so a still status bar costs nothing. A smaller view (the `-` key) or Low detail in DOOM's Options draws faster. On x86, **Game > Fast Drawing (AVX2)**, on at the start, draws walls, sprites and floors and turns the picture into the window's colours with the AVX2 loops of `os/doom/doom_simd.asm`, eight pixels at a time: the emulator's time goes by instructions, and a `vpgatherdd` fetches eight texels in one where C takes a dozen instructions for each. The editor's C compiler makes no AVX, so they are written by hand, as `os/platform/x86/blit.asm` is; turning the item off goes back to DOOM's own C loops, to compare. **Game > Profile**, on x86, lists the functions the processor is in most often while DOOM runs, sampled a hundred times a second by the kernel's timer (`os_profile`) and named from the program's own symbol table, which it reads from the Build output `os/main`. **Game > Benchmark CPU**, on x86, times each kind of x86 instruction in the emulator, the cost every one of these choices was weighed against. RISC-V and MIPS have none of the three: see [On RISC-V and MIPS](#on-risc-v-and-mips).
- **Another game.** DOOM keeps its world in variables that start as the compiler set them and never sets them back. So each of its four parts is assembled by an `os/doom/doom_*.part.asm` File between labels that mark where its variables lie, and before a game `os/doom/asmos_fresh.asm` puts back the values they started with, which the first game kept. Close DOOM and open it again, and the second game begins as the first did. One DOOM runs at a time. RISC-V and MIPS have no such assembly, so there DOOM plays once each time asmOS starts, and opening it again says to restart asmOS.

doomgeneric, DOOM's source and asmOS's platform for it are under the GNU General Public License 2, in `os/doom/LICENSE.txt`.

## Assembly Editor

The Assembly Editor writes, builds, runs and debugs RISC-V programs inside asmOS: RV32IM assembly as RARS 1.6 assembles and runs it, so a `.riscv` File runs the same here and in the web editor's RISC-V Target. Open it from the desktop, with `asm FILE` in the Terminal, or by opening a `.riscv` File. File > Examples opens the programs in `code/`: hello, a table of Fibonacci numbers, a keyboard echo, a guessing game, a ball bouncing on the bitmap display, and a speed test.

- **Running.** Build (F3) assembles the program and lists its errors under Problems: click one to go to its line. Run (F5) runs it, Pause (F9) stops it, Step (F7) runs one instruction and Step back (F8) takes one back, what it printed included. Click the gutter or press F2 for a breakpoint. The keys are RARS's, F10 being asmOS's menu key. A program runs in slices of a few milliseconds, so the desktop and the editor keep going while it runs.
- **What it shows.** The registers, those the last step changed in red; the memory from the data segment, from the stack pointer or from an address or label; the console, which also takes what read integer, read string and read character wait for, and the keys the Keyboard and Display MMIO reads; and the bitmap display that a comment such as `# @screen width=256 height=256 unit=4 base=display` asks for, read as the web editor reads it, beside the registers or in a window of its own (View).
- **The machine.** RARS's memory map (text at 0x00400000, static data at 0x10010000, the heap from 0x10040000, the stack from 0x7fffeffc), RARS's pseudo-instructions and directives, its ecalls for printing, reading, sbrk, exit, time, sleep, MIDI notes and random numbers (Java's generator, so a seeded program prints what RARS prints), and the Keyboard and Display MMIO at 0xffff0000. Floating point, CSRs, macros, `.include`, the file ecalls and the dialogs are not there. Help > Ecalls and Help > Devices list what is.
- **Speed.** On x86, with the Program's Undo off, as this Project has it, about 400,000 RISC-V instructions a second, and twice that with Run > Keep History for Step Back turned off; with Undo on, about 90,000 and 200,000. On RISC-V about 200,000 a second and on MIPS about 130,000, in a browser, which runs the emulator's WebAssembly, and half that on its JavaScript: the emulator there runs six to fourteen million instructions a second, and drawing the editor's window takes most of them, so a slice of the program lasts as long as the drawing between two slices does, up to a tenth of a second. `code/speed.riscv` measures it.
- **The files.** `os/ide/rv_asm.c` is the assembler and `os/ide/rv_cpu.c` the interpreter, plain C with nothing of asmOS, their interface `os/ide/rv.h`; `os/ide/rvcli.c` builds the two into a program for your own computer, which the tests ran beside RARS over a hundred programs, comparing the machine code, the output, the registers and how each one ended. `os/ide/ide.c` is the window. It is compiled apart, as DOOM is.
- Programs are `.riscv` Files, the web editor's name for RISC-V assembly: on x86 a Build of asmOS would assemble a `.s` or `.asm` File with NASM.

## Bad Apple!!

The Video Player opens `videos/bad-apple.vid` when it starts: Bad Apple!!, the shadow-play video that the demo scene plays on every machine it can, 3 minutes 39 seconds of 480 × 360 black and white at 30 frames a second, and its song, in 2.4 MB. Space or a click on the picture pauses it, the bar under it seeks, Left and Right jump five seconds, S turns the sound off, and the window resizes and maximizes, the picture scaled to fit.

- **The picture.** A frame is rows of black and white, and a row is stored as its changes, the x of each pixel whose colour differs from the one before it. Each row is coded the way a fax machine codes a page (ITU-T T.6): against a reference row that is nearly the same, so that most changes are "the reference's change", or that change a pixel or two away. The reference is the same row of the previous frame, perhaps a few rows up or down when the picture moves, or the row above. Every decision goes through an adaptive binary range coder, LZMA's, so a change where the reference's was costs a fraction of a bit, and the picture comes to about 170 bytes a frame.
- **The sound** is ADPCM at 16,000 samples a second, laid out as the QOA format lays it out: a predictor guesses each sample from the four before it, and each slice of 20 samples keeps only the differences from the guesses, 3 bits each, on a scale of its own.
- **The clock.** While it plays, a timer every 10 ms keeps `/dev/dsp` full, or the sampled-audio output on RISC-V and MIPS, and makes the frames that the sound played so far has reached, so picture and sound keep together however the processor is shared. Only the rows that changed are drawn, a fill for each run of black or white. Making a frame takes about 110,000 instructions and a second of sound about a million: with the drawing, the video takes about half of what the emulator runs, and the rest of asmOS keeps running beside it.
- **Seeking.** A `.vid` is cut into chunks of five seconds, each coded from a black frame and from silence, so the player can start at any one: that is where a seek lands.
- **The files.** `os/apps/video/vid.c` reads the format; its comment describes it byte by byte. `os/apps/video/encode.c` is not part of asmOS but a program for your own computer, which made `bad-apple.vid` and makes a `.vid` of any video from the frames and the sound ffmpeg extracts: its comment has the commands. It reads back what it wrote with `vid.c` itself, so a file it makes is one asmOS plays.

Bad Apple!! is a video by Anira to "Bad Apple!! feat. nomico", Alstroemeria Records' arrangement of ZUN's "Bad Apple!!" from the Touhou Project, sung by nomico. They are their authors' own, not under asmOS's license.

## On RISC-V and MIPS

asmOS runs on the editor's RARS and MARS as one program, from the same tree as on x86: the platform layer, `os/platform/`, is all that differs. `os/platform/sim/` holds what the two machines do alike and `os/platform/riscv/` and `os/platform/mips/` what is each processor's own.

- **The screen** is the bitmap display, 640 × 480 words of `0x00RRGGBB` in static data, which `SIM_SCREEN` declares (`sim/screen.c`): a word stored there is a pixel on the Screen.
- **Memory.** The editor's MARS and RARS give static data and the heap 64 MB, where MARS and RARS themselves give 4: room for the screen, a back buffer the window manager composes in, the wallpaper's picture, the Video Player's video and DOOM, whose IWAD read whole and its own memory take 10.4 MB, and for x86's process table, 32 processes of 128 waiting messages each. Each process's stack, 64 KB, is a slot at the far end of the 4 MB stack segment, which costs the heap nothing (`sim/memory.c`): the deepest an application's stack went, measured, was 12 KB, DOOM's. About 62 MB are left for the applications when asmOS has started, and 50 MB while DOOM plays.
- **The keyboard and the mouse** are the editor's Input controller, a device at 0xFFFF0300 (`sim/input.c`): it reports keys as USB HID usages, which a table turns into asmOS's key codes as Linux's HID driver turns them into its own, button presses and the wheel in one queue with where the pointer was, and the pointer's position. asmOS sleeps between two polls of it.
- **The timer** takes the processor from an application that has run for 10 ms, as Linux's timer signal does on x86, and the switch is a trap handler in assembly (see [Processes](#processes)). On RISC-V the timer is RARS's Timer Tool, `time` and `timecmp`, which count the milliseconds the program has run. On MIPS it is coprocessor 0's Count and Compare: Count goes up once every two instructions, not with the clock, so asmOS measures how many make a millisecond as it starts, against syscall 30's clock, and says so on the Terminal.
- **The clock** is syscall 30 and sleep syscall 32; the count of instructions run, which Task Manager shows, is RISC-V's `instret` counter, and on MIPS the editor's syscall 1101.
- **The files** go through the C library's POSIX calls, made of MARS's and RARS's file and folder calls, the same `os/platform/files.c` as on x86.
- **Sound** is the two devices the editor adds to MARS and RARS (`sim/sound.c`): the MIDI port at 0xFFFF0340, a General MIDI synthesizer's, which plays the Music Player, Blocks and the chimes as x86's MIDI port does, and the sampled-audio output at 0xFFFF0350, a queue of samples that Pac-Man's mixer and the Video Player fill as they fill `/dev/dsp` on x86. MARS and RARS themselves have neither.
- **Bad Apple!! is not in the MIPS Project**, for now: with DOOM, MIPS's Generated assembly, the largest of the three machines', would take the Project past the 16 MiB of Files a Project may hold, so `videos/` is left out of it. The Video Player is still there, and starts with nothing to play.
- **DOOM** draws, turns its picture into the window's colours and mixes its sound in its own C, as on x86 with Fast Drawing off: a frame takes about 2.5 million instructions, so over its first attract demo DOOM shows 4.9 frames a second on RISC-V and 3.8 on MIPS in a browser, which runs the emulator's WebAssembly, and 2.3 and 1.9 on its JavaScript build, with the Program's Undo off. DOOM runs only a few tics of its game ahead of what it has shown, so at that speed its world moves slower than it should, at about two thirds of its pace in a browser and a third on the JavaScript builds. Its first attract demo plays as on x86, tic for tic: 5,026 tics, sum `cf6b7f4d`. Its sound goes to the sampled-audio output and its music to the MIDI port, as they go to `/dev/dsp` and the MIDI port on x86. Game > Fast Drawing, Profile and Benchmark CPU are x86's own and not in its menu (`os/doom/asmos_x86.h`). And DOOM plays once each time asmOS starts: these machines have no `asmos_fresh.asm` to set its variables back for a second game, so opening it again says to restart asmOS.
- **Speed.** The emulator runs six to fourteen million instructions a second, the more in a browser, which runs its WebAssembly, with the Program's **Undo enabled** off, as these Projects have it: the history of every instruction nearly halves the speed of drawing. Every pixel is stored by a C loop where x86 has `rep` instructions, and what the back buffer shows is copied to the screen by one more, so drawing is what costs: asmOS starts in 2 to 2.5 seconds, Notepad opens in 0.4 to 0.8 seconds and the Assembly Editor in 0.6 to 1.3, a key in it shows within a tenth of a second, and a step of a window being dragged in a fifth to a third. An application that computes without ever waiting, a Terminal sorting 30,000 lines say, no longer holds the desktop up: the timer takes the processor from it, and the Start menu opens in about 0.15 seconds meanwhile. The Video Player keeps up with its sound. The first Build takes ten seconds or so.

## How it works

### Processes

Each application is a process: its `..._main` function runs on a stack of its own (256 KB on x86, 64 KB on RISC-V and MIPS), with its own message queue and its own memory, which `os_alloc` charges to it and which is freed when it ends. The bottom 256 bytes of each stack are a guard the kernel fills as the process starts and looks at each time it switches from it: a process that has written into it has used all of its stack, and the system ends it with a message rather than let it write on into another process's stack. And where the heap has a limit, as on RISC-V and MIPS, `os_alloc` answers NULL for memory the heap has no room left for, so that the application that asked fails, not asmOS: the machine would end the whole program. Process 0, the system, is `main`'s loop: it reads the keyboard and the pointer, turns them into messages for the windows' processes, fires timers and composes the screen.

One process runs at a time. On x86 the kernel switches in a signal handler (`os/platform/x86/switch.c`):

- A process that waits for a message, sleeps or asks a dialog something gives the processor up by sending itself `SIGUSR2`.
- While applications are ready to run, the interval timer sends `SIGALRM` every 10 milliseconds, which takes the processor from one that never gives it up: **preemption**. A process stuck in a loop cannot freeze the desktop, and Task Manager can end it.

Either way, the handler receives the interrupted process's registers in the signal frame: the general-purpose registers, and the 512 bytes of x87 and SSE state. It saves them into the process and writes the next process's into the frame, so the return from the handler resumes the next process exactly where it stopped. A new process's first turn starts it at `process_start` on its own stack.

On RISC-V and MIPS the switch is a trap handler written in assembly, `os/platform/riscv/trap.riscv` and `os/platform/mips/trap.mips`, which the processor enters between two instructions of whatever runs:

- The timer's interrupt, while applications are ready to run: on RISC-V once `time` reaches `timecmp`, which the handler then sets 10 ms on, and on MIPS once Count reaches Compare.
- A process that gives the processor up traps on purpose: on RISC-V it asks for a user software interrupt (`csrsi uip, 1`), since `ecall` asks RARS for a service, and on MIPS it runs a `teq` that always traps, since `syscall` asks MARS for one, and which the handler knows by its address.
- Any other exception, a load from an address that is not there say, ends asmOS with a line on the Terminal saying what it was, where and in which process.

The handler keeps every register of the stopped process in a frame on that process's own stack, the general-purpose and floating-point ones and where it stopped (`uepc` on RISC-V, EPC on MIPS). It asks the kernel, in C (`switch.c` beside it), which process goes on, takes that process's stack, loads its registers back from the frame there, and returns into it with `uret` or `eret`. A new process's first turn is a frame made to look the same, whose pc is `process_start`. On RISC-V the program installs the handler by putting its address in `utvec`; on MIPS the handler is in `.ktext`, at 0x80000180, where the processor enters it. The Build links the `.riscv` File because the kernel calls the functions it defines, and the `.mips` File because it holds `.ktext`.

The yield points of the long loops (`os_yield_point`) are for a machine without a timer, as x86 tries when built with `-DASMOS_COOPERATIVE`: there a process gives the processor up with a plain call, which keeps the registers a called function must keep (`os/platform/x86/switch.asm`). Elsewhere they cost nothing.

A process can be stopped at any instruction, so the kernel's own data is changed only under the **kernel lock**: an `os_` call takes it, and the timer leaves a process that holds it alone until it lets go. Applications therefore never call `malloc`, `printf` or `fopen` themselves, only the `os_` calls `os/os.h` lists, and the pure parts of the C library.

### Windows

Every window has a picture of its own, its client area, which its process draws into whenever it likes. The compositor (`wm.c`) never asks a process to draw. When something changes, a rectangle of the screen is marked dirty, and once a pass the compositor repaints the dirty rectangles into a back buffer, from the desktop up through the windows (their frames, and their pictures copied in) to the taskbar and the menus, then copies them onto the framebuffer. A window that covers a dirty rectangle completely is painted alone. The pointer, a dragged file and a resize outline are drawn on the framebuffer only, over what the compositor put there. A dragged window's pixels move as they are, and only the strip it uncovers is repainted. A dirty rectangle that lies inside the top window's picture, under no menu, goes straight from that picture to the framebuffer, as DOOM's frames do: the back buffer catches up there only when something reads it.

Built without a back buffer, for a machine short of memory (`-DPLATFORM_BACK_BUFFER=0` tries it on x86), the dirty rectangles are painted straight onto the screen, and what is drawn over them, the pointer and a resize outline, is taken away by painting again what lies under it. A dragged window's pixels move on the screen itself, the pointer first taken off them.

A resize gives the window a new picture. The old one is kept until the process takes the `MSG_RESIZE` message, since until then the process may still be drawing into it.

### Assembly

`os/platform/x86/blit.asm` fills and copies rectangles of pixels. One `rep` instruction does a whole row, and the emulator runs it as a single instruction: filling the screen takes a few thousand instructions, where a C loop storing one pixel at a time takes over 700,000. Of the `rep` instructions `rep movsb` runs fastest by far, so rows are copied by bytes, and a fill stores its first row with `rep stosd` and copies it into the others. RISC-V and MIPS have no such instructions, and their pixel loops are C, `os/blit.c`: there the assembly is the trap handler.

### Files

The code is in `os/`, in four parts compiled apart: the system in `os/` itself, the applications in `os/apps/`, the Assembly Editor in `os/ide/` and DOOM in `os/doom/`. Everything else in the Project is asmOS's own disk, the files it shows and opens: `Desktop`, `code`, `games`, `music`, `pictures`, `videos`, `welcome.txt`, and the `asmos.ini` Settings writes.

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
| `os/platform/platform.h`, `platform.c`, `files.c` | The platform layer: what asmOS asks of the machine, the machine's Files chosen, and the files through POSIX |
| `os/platform/x86/` | x86: Linux's framebuffer, input devices, clocks, sound and signals, and `blit.asm` |
| `os/platform/sim/` | What RISC-V and MIPS share on RARS and MARS: the bitmap display, the Input controller, the clock, the stacks, the MIDI port and the sampled-audio output |
| `os/platform/riscv/`, `os/platform/mips/` | Each processor's trap handler, in assembly (`trap.riscv`, `trap.mips`), its C half with the timer (`switch.c`), and its instruction counter |
| `os/bmp.c` | Reading and writing BMP pictures |
| `os/font.c`, `os/icons.c` | The 8 × 13 font (X11's public-domain "fixed") and the icons |
| `os/apps/apps.c` | The applications' half: the application table, and the applications beside it in `os/apps/` |
| `os/apps/video.c`, `os/apps/video/vid.c` | The Video Player, and the `.vid` format it plays |
| `os/apps/video/encode.c` | The `.vid` encoder: a program for your own computer, not compiled with asmOS |
| `os/ide/ide.c` | The Assembly Editor's window, which includes the two Files below |
| `os/ide/rv.h`, `os/ide/rv_asm.c`, `os/ide/rv_cpu.c` | The RISC-V assembler and interpreter, plain C without asmOS |
| `os/ide/rvcli.c` | The two as a program for your own computer, to test them beside RARS: not compiled with asmOS |
| `os/doom/doom_*.c`, `os/doom/doom_*.part.asm` | DOOM's four parts, and on x86 the Files that assemble them between the labels of their variables |
| `os/doom/` | DOOM's platform for asmOS: `i_asmos.c`, `i_asmos_sound.c`, `doomlibc.c`, the x86 assembly loops and the other `asmos_` Files; `asmos_x86.h` says what DOOM goes without on RISC-V and MIPS |
| `os/doom/doomgeneric/` | The engine, doomgeneric's own Files, where a comment marks each change with `asmOS:` |

## Compiling it again

On x86 the two halves are compiled apart, each into its own Generated assembly, with **-O1**: `os/main.c` into `os/main.c.asm`, the Program's Entry, and `os/apps/apps.c` into `os/apps/apps.c.asm`. The Assembly Editor is one more, with **-O2**, so that its interpreter runs fast: `os/ide/ide.c` into `os/ide/ide.c.asm`. DOOM is too large for one compilation, Compiler Explorer taking 1 MiB of source at a time, so it is four more, with **-O2**: `os/doom/doom_game.c`, `doom_play.c`, `doom_render.c` and `doom_world.c`, each assembled by its `doom_*.part.asm`. The Build links them by the names `os/os.h` and DOOM's headers declare. All seven come with the Project, so Build works without compiling. After changing a File, select the one of the seven that includes it and press **Compile**: `os/main.c` for a File of `os/`, `os/apps/apps.c` for one of `os/apps/`, `os/ide/ide.c` for one of `os/ide/`, and for one of `os/doom/` the `doom_*.c` that lists it.

On RISC-V and MIPS the same seven are compiled with Clang into `os/main.c.riscv` and the others, or `.mips`: `os/main.c` and `os/ide/ide.c` at **-O2**, and `os/apps/apps.c` and DOOM's four parts at **-Os**. Compiler Explorer's answer for `apps.c` at -O2 would be over the 16 MiB the editor takes, and DOOM runs no slower at -Os while its Generated assembly is smaller, which keeps the Project within the 16 MiB of Files a Project may hold. DOOM's four parts need no `.part.asm` there: they are linked as they are, as the other three are, with the trap handler's File of the processor's folder, which is assembly already.

Before compiling one of them again on RISC-V or MIPS, turn **Include source annotations when compiling** off in the editor's Settings. The editor's Compile adds them by default, and with them the Generated assembly takes the Project past the 16 MiB of Files a Project may hold. The Generated assembly that comes with the Project is compiled without them, as the editor's `scripts/examples/recompile.task.ts` compiles it.
