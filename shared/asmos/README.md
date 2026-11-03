# asmOS

A small operating system for the x86 Screen: a kernel that runs every application as its own process and switches between them, a compositing window manager with menus, dialogs, drag and drop and a clipboard, a desktop with a Start menu, and eighteen applications, Pac-Man and DOOM among them, a Video Player that plays Bad Apple!!, and an Assembly Editor that runs RISC-V programs. It is written in C, with its pixel loops in x86 assembly.

asmOS is not a real kernel: it runs as one ordinary Linux program on the emulated machine. Its processes, scheduler, memory accounting and window system are its own, built on what Linux gives a program: the framebuffer `/dev/fb0`, the keyboard and the mouse under `/dev/input`, sound on the MIDI port and `/dev/dsp`, timers and signals, and the Project's files as its working directory.

## Running it

1. Select the Program `os/main.c.asm` and press **Build**, then **Run**.
2. Click the Screen, so that it gets the keyboard and the mouse. **Shift+Tab** gives the keyboard back to the page.
3. Double-click an icon, open the **Start** menu, or right-click the desktop. **Shut Down** ends the program.

The Screen is 640 × 480, set by the `@screen` comment at the top of `os/main.c`. List applications in an `autostart.txt` File, one a line (`notepad welcome.txt`), to have asmOS start them.

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
| Pac-Man | The arcade game, ported from the RISC-V example, its sound mixed in C and played through `/dev/dsp`. |
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
- **Speed.** The emulator runs a few million instructions a second and a frame of DOOM takes about two million, so DOOM shows a few frames a second. Turn **Undo enabled** off in the Program's Settings, which this Project does: keeping the history of every instruction makes DOOM three times slower. The frames a second show in the picture's top right corner (Game > FPS Counter hides them), and Game > Frame Times shows over the picture where a frame goes, in milliseconds and in thousands of instructions (`os_instructions`, the emulator's own count): L the game's logic, T its thinkers and within them sight and move, W walls, F floors, M sprites, O DOOM's other drawing and bar its status bar, S the copy into the window and within it pal the palette and dbl the repeated rows, I messages and sound; and how many millions of instructions a second DOOM, the system and all ran, and a sum of the last attract demo's tics, which must not change from one version of DOOM to the next. Only the rows of DOOM's picture that changed since the last frame are turned into colours and shown again, so a still status bar costs nothing. A smaller view (the `-` key) or Low detail in DOOM's Options draws faster. **Game > Fast Drawing (AVX2)**, on at the start, draws walls, sprites and floors and turns the picture into the window's colours with the AVX2 loops of `os/doom/doom_simd.asm`, eight pixels at a time: the emulator's time goes by instructions, and a `vpgatherdd` fetches eight texels in one where C takes a dozen instructions for each. The editor's C compiler makes no AVX, so they are written by hand, as `os/blit.asm` is; turning the item off goes back to DOOM's own C loops, to compare. **Game > Profile** lists the functions the processor is in most often while DOOM runs, sampled a hundred times a second by the kernel's timer (`os_profile`) and named from the program's own symbol table, which it reads from the Build output `os/main`. **Game > Benchmark CPU** times each kind of instruction in the emulator, the cost every one of these choices was weighed against.
- **Another game.** DOOM keeps its world in variables that start as the compiler set them and never sets them back. So each of its four parts is assembled by an `os/doom/doom_*.part.asm` File between labels that mark where its variables lie, and before a game `os/doom/asmos_fresh.asm` puts back the values they started with, which the first game kept. Close DOOM and open it again, and the second game begins as the first did. One DOOM runs at a time.

doomgeneric, DOOM's source and asmOS's platform for it are under the GNU General Public License 2, in `os/doom/LICENSE.txt`.

## Assembly Editor

The Assembly Editor writes, builds, runs and debugs RISC-V programs inside asmOS: RV32IM assembly as RARS 1.6 assembles and runs it, so a `.riscv` File runs the same here and in the web editor's RISC-V Target. Open it from the desktop, with `asm FILE` in the Terminal, or by opening a `.riscv` File. File > Examples opens the programs in `code/`: hello, a table of Fibonacci numbers, a keyboard echo, a guessing game, a ball bouncing on the bitmap display, and a speed test.

- **Running.** Build (F3) assembles the program and lists its errors under Problems: click one to go to its line. Run (F5) runs it, Pause (F9) stops it, Step (F7) runs one instruction and Step back (F8) takes one back, what it printed included. Click the gutter or press F2 for a breakpoint. The keys are RARS's, F10 being asmOS's menu key. A program runs in slices of a few milliseconds, so the desktop and the editor keep going while it runs.
- **What it shows.** The registers, those the last step changed in red; the memory from the data segment, from the stack pointer or from an address or label; the console, which also takes what read integer, read string and read character wait for, and the keys the Keyboard and Display MMIO reads; and the bitmap display that a comment such as `# @screen width=256 height=256 unit=4 base=display` asks for, read as the web editor reads it, beside the registers or in a window of its own (View).
- **The machine.** RARS's memory map (text at 0x00400000, static data at 0x10010000, the heap from 0x10040000, the stack from 0x7fffeffc), RARS's pseudo-instructions and directives, its ecalls for printing, reading, sbrk, exit, time, sleep, MIDI notes and random numbers (Java's generator, so a seeded program prints what RARS prints), and the Keyboard and Display MMIO at 0xffff0000. Floating point, CSRs, macros, `.include`, the file ecalls and the dialogs are not there. Help > Ecalls and Help > Devices list what is.
- **Speed.** With the Program's Undo off, as this Project has it, about 400,000 RISC-V instructions a second, and twice that with Run > Keep History for Step Back turned off; with Undo on, about 90,000 and 200,000. `code/speed.riscv` measures it.
- **The files.** `os/ide/rv_asm.c` is the assembler and `os/ide/rv_cpu.c` the interpreter, plain C with nothing of asmOS, their interface `os/ide/rv.h`; `os/ide/rvcli.c` builds the two into a program for your own computer, which the tests ran beside RARS over a hundred programs, comparing the machine code, the output, the registers and how each one ended. `os/ide/ide.c` is the window. It is compiled apart, as DOOM is.
- Programs are `.riscv` Files, the web editor's name for RISC-V assembly: a Build of asmOS would assemble a `.s` or `.asm` File with NASM, as x86.

## Bad Apple!!

The Video Player opens `videos/bad-apple.vid` when it starts: Bad Apple!!, the shadow-play video that the demo scene plays on every machine it can, 3 minutes 39 seconds of 480 × 360 black and white at 30 frames a second, and its song, in 2.4 MB. Space or a click on the picture pauses it, the bar under it seeks, Left and Right jump five seconds, S turns the sound off, and the window resizes and maximizes, the picture scaled to fit.

- **The picture.** A frame is rows of black and white, and a row is stored as its changes, the x of each pixel whose colour differs from the one before it. Each row is coded the way a fax machine codes a page (ITU-T T.6): against a reference row that is nearly the same, so that most changes are "the reference's change", or that change a pixel or two away. The reference is the same row of the previous frame, perhaps a few rows up or down when the picture moves, or the row above. Every decision goes through an adaptive binary range coder, LZMA's, so a change where the reference's was costs a fraction of a bit, and the picture comes to about 170 bytes a frame.
- **The sound** is ADPCM at 16,000 samples a second, laid out as the QOA format lays it out: a predictor guesses each sample from the four before it, and each slice of 20 samples keeps only the differences from the guesses, 3 bits each, on a scale of its own.
- **The clock.** While it plays, a timer every 10 ms keeps `/dev/dsp` full and makes the frames that the sound played so far has reached, so picture and sound keep together however the processor is shared. Only the rows that changed are drawn, a fill for each run of black or white. Making a frame takes about 110,000 instructions and a second of sound about a million: with the drawing, the video takes about half of what the emulator runs, and the rest of asmOS keeps running beside it.
- **Seeking.** A `.vid` is cut into chunks of five seconds, each coded from a black frame and from silence, so the player can start at any one: that is where a seek lands.
- **The files.** `os/apps/video/vid.c` reads the format; its comment describes it byte by byte. `os/apps/video/encode.c` is not part of asmOS but a program for your own computer, which made `bad-apple.vid` and makes a `.vid` of any video from the frames and the sound ffmpeg extracts: its comment has the commands. It reads back what it wrote with `vid.c` itself, so a file it makes is one asmOS plays.

Bad Apple!! is a video by Anira to "Bad Apple!! feat. nomico", Alstroemeria Records' arrangement of ZUN's "Bad Apple!!" from the Touhou Project, sung by nomico. They are their authors' own, not under asmOS's license.

## How it works

### Processes

Each application is a process: its `..._main` function runs on a stack of its own (256 KB), with its own message queue and its own memory, which `os_alloc` charges to it and which is freed when it ends. Process 0, the system, is `main`'s loop: it reads the keyboard and the pointer, turns them into messages for the windows' processes, fires timers and composes the screen.

One process runs at a time. The kernel switches in a signal handler (`kernel.c`):

- A process that waits for a message, sleeps or asks a dialog something gives the processor up by sending itself `SIGUSR2`.
- While applications are ready to run, the interval timer sends `SIGALRM` every 10 milliseconds, which takes the processor from one that never gives it up: **preemption**. A process stuck in a loop cannot freeze the desktop, and Task Manager can end it.

Either way, the handler receives the interrupted process's registers in the signal frame: the general-purpose registers, and the 512 bytes of x87 and SSE state. It saves them into the process and writes the next process's into the frame, so the return from the handler resumes the next process exactly where it stopped. A new process's first turn starts it at `process_start` on its own stack.

A process can be stopped at any instruction, so the kernel's own data is changed only under the **kernel lock**: an `os_` call takes it, and the timer leaves a process that holds it alone until it lets go. Applications therefore never call `malloc`, `printf` or `fopen` themselves, only the `os_` calls `os/os.h` lists, and the pure parts of the C library.

### Windows

Every window has a picture of its own, its client area, which its process draws into whenever it likes. The compositor (`wm.c`) never asks a process to draw. When something changes, a rectangle of the screen is marked dirty, and once a pass the compositor repaints the dirty rectangles into a back buffer, from the desktop up through the windows (their frames, and their pictures copied in) to the taskbar and the menus, then copies them onto the framebuffer. A window that covers a dirty rectangle completely is painted alone. The pointer, a dragged file and a resize outline are drawn on the framebuffer only, over what the compositor put there. A dragged window's pixels move as they are, and only the strip it uncovers is repainted. A dirty rectangle that lies inside the top window's picture, under no menu, goes straight from that picture to the framebuffer, as DOOM's frames do: the back buffer catches up there only when something reads it.

A resize gives the window a new picture. The old one is kept until the process takes the `MSG_RESIZE` message, since until then the process may still be drawing into it.

### Assembly

`os/blit.asm` fills and copies rectangles of pixels. One `rep` instruction does a whole row, and the emulator runs it as a single instruction: filling the screen takes a few thousand instructions, where a C loop storing one pixel at a time takes over 700,000. Of the `rep` instructions `rep movsb` runs fastest by far, so rows are copied by bytes, and a fill stores its first row with `rep stosd` and copies it into the others.

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
| `os/input.c` | The keyboard and the pointer, read as Linux input events |
| `os/sound.c` | Notes for the General MIDI synthesizer, on the raw MIDI port |
| `os/bmp.c` | Reading and writing BMP pictures |
| `os/font.c`, `os/icons.c` | The 8 × 13 font (X11's public-domain "fixed") and the icons |
| `os/apps/apps.c` | The applications' half: the application table, and the applications beside it in `os/apps/` |
| `os/apps/video.c`, `os/apps/video/vid.c` | The Video Player, and the `.vid` format it plays |
| `os/apps/video/encode.c` | The `.vid` encoder: a program for your own computer, not compiled with asmOS |
| `os/ide/ide.c` | The Assembly Editor's window, which includes the two Files below |
| `os/ide/rv.h`, `os/ide/rv_asm.c`, `os/ide/rv_cpu.c` | The RISC-V assembler and interpreter, plain C without asmOS |
| `os/ide/rvcli.c` | The two as a program for your own computer, to test them beside RARS: not compiled with asmOS |
| `os/doom/doom_*.c`, `os/doom/doom_*.part.asm` | DOOM's four parts, and the Files that assemble them between the labels of their variables |
| `os/doom/` | DOOM's platform for asmOS: `i_asmos.c`, `i_asmos_sound.c`, `doomlibc.c`, the assembly loops and the other `asmos_` Files |
| `os/doom/doomgeneric/` | The engine, doomgeneric's own Files, where a comment marks each change with `asmOS:` |

## Compiling it again

The two halves are compiled apart, each into its own Generated assembly, with **-O1**: `os/main.c` into `os/main.c.asm`, the Program's Entry, and `os/apps/apps.c` into `os/apps/apps.c.asm`. The Assembly Editor is one more, with **-O2**, so that its interpreter runs fast: `os/ide/ide.c` into `os/ide/ide.c.asm`. DOOM is too large for one compilation, Compiler Explorer taking 1 MiB of source at a time, so it is four more, with **-O2**: `os/doom/doom_game.c`, `doom_play.c`, `doom_render.c` and `doom_world.c`, each assembled by its `doom_*.part.asm`. The Build links them by the names `os/os.h` and DOOM's headers declare. All seven come with the Project, so Build works without compiling. After changing a File, select the one of the seven that includes it and press **Compile**: `os/main.c` for a File of `os/`, `os/apps/apps.c` for one of `os/apps/`, `os/ide/ide.c` for one of `os/ide/`, and for one of `os/doom/` the `doom_*.c` that lists it.
