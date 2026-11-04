/*
 * What asmOS's MIPS and RISC-V ports share: both run on the editor's MARS and RARS, whose machines
 * are the same apart from the processor. One program with 64 MB of static data and heap and a 4 MB
 * stack segment, a bitmap display that is a grid of words in memory, the editor's Input
 * controller for the keyboard and the mouse, its MIDI port and sampled-audio output, and syscalls
 * for the clock, sleep and the Project's files. The Files beside this one do the platform layer's
 * parts the two machines do alike:
 *
 *   screen.c    the bitmap display, a grid of 0x00RRGGBB words in static data (SIM_SCREEN)
 *   input.c     the keyboard and the mouse, from the Input controller at 0xFFFF0300
 *   time.c      the clock and sleep, syscalls 30 and 32
 *   memory.c    the stacks, from the stack segment, and what the heap has left
 *   sound.c     the MIDI port at 0xFFFF0340 and the sampled-audio output at 0xFFFF0350
 *
 * The processor's own parts, the timer, the trap handler that switches processes and the
 * instruction counter, are in os/platform/riscv/ and os/platform/mips/, whose header includes
 * this one.
 */
#ifndef ASMOS_SIM_MACHINE_H
#define ASMOS_SIM_MACHINE_H

/* Each processor has a timer whose interrupt takes the processor from a process, as Linux's timer
 * signal does on x86: RISC-V's timecmp, MIPS's Count and Compare. The switch is in a trap handler
 * written in assembly (trap.riscv, trap.mips). */
#define PLATFORM_PREEMPTIVE 1

/*
 * Memory. Static data and the heap share 64 MB, 0x10010000 to 0x14000000, which the screen, the
 * back buffer, the wallpaper's picture and the Video Player's video each take a megabyte or more
 * of; the kernel's tables are x86's, 0.8 MB of static data. The stacks come from the 4 MB stack
 * segment instead (memory.c): 32 slots of 64 KB at its far end, and its top half for the system's
 * own stack, main's. Measured, the deepest an application's stack went was 12 KB, DOOM's, so a slot
 * leaves room for a trap frame at the deepest point and more. The kernel fills the bottom of each
 * slot with a guard, and ends a process that has written into it (kernel.c).
 */
#define PLATFORM_MAX_PROCESSES 32
#define PLATFORM_QUEUE_SIZE 128         /* the messages that can wait for each process */
#define PLATFORM_STACK_SIZE (64 * 1024)

/* The Project's folders, through the folder calls MARS and RARS gained for asmOS (files.c). */
#define PLATFORM_FILE_SYSTEM (FS_LIST | FS_DELETE | FS_RENAME | FS_MAKE_DIR)
/* No pixel loops of the machine's own: os/blit.c's C ones. */
#define PLATFORM_BLIT 0
/* A back buffer the window manager composes in, and a picture as the wallpaper. What the back
 * buffer shows is copied to the screen by C, about three instructions a pixel where x86 copies a
 * row in one, so it doubles what a redraw runs here; -DPLATFORM_BACK_BUFFER=0 tries drawing
 * straight onto the screen instead, as a machine short of memory would, with no wallpaper picture. */
#ifndef PLATFORM_BACK_BUFFER
#define PLATFORM_BACK_BUFFER 1
#endif
#define PLATFORM_WALLPAPER_PICTURE PLATFORM_BACK_BUFFER
/* The Video Player reads its whole 2.5 MB video at once, and DOOM its 4 MB IWAD, beside 6 MB of
 * memory of its own, which 64 MB holds. DOOM draws in its own C here, and plays once each time
 * asmOS starts (os/doom/asmos_x86.h). */
#define PLATFORM_DOOM 1
#define PLATFORM_VIDEO 1

/* A process stopped by the trap handler keeps its registers on its own stack, in the frame the
 * handler saved them in (switch.c of the processor's folder): only where that is. */
typedef struct {
    void *sp;
} PlatformContext;

#endif
