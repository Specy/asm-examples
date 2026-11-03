/*
 * What asmOS's MIPS and RISC-V ports share: both run on the editor's MARS and RARS, whose machines
 * are the same apart from the processor. One program in a 4 MB data segment and a 4 MB stack
 * segment, a bitmap display that is a grid of words in memory, the editor's Input controller for
 * the keyboard and the mouse, and syscalls for the clock, sleep and the Project's files. The
 * Files beside this one do the platform layer's parts the two machines do alike:
 *
 *   screen.c    the bitmap display, a grid of 0x00RRGGBB words in static data (SIM_SCREEN)
 *   input.c     the keyboard and the mouse, from the Input controller at 0xFFFF0300
 *   time.c      the clock and sleep, syscalls 30 and 32
 *   memory.c    the stacks, from the far end of the stack segment, and what the heap has left
 *   sound.c     none: asmOS is silent here
 *
 * The processor's own parts, the switch between processes and the instruction counter, are in
 * os/platform/riscv/ and os/platform/mips/, whose header includes this one.
 */
#ifndef ASMOS_SIM_MACHINE_H
#define ASMOS_SIM_MACHINE_H

/* No timer can take the processor from a process, so every process gives it up itself: when it
 * waits, and at the yield points of its long loops. The switch is a call (switch.c). */
#define PLATFORM_PREEMPTIVE 0

/*
 * Memory. Static data and the heap share 4 MB, 0x10010000 to 0x10400000, and the screen alone
 * takes 1.2 MB of it, so the kernel's tables are small: a process's message queue is most of its
 * entry in the process table, and 32 processes of 128 messages would take 0.8 MB. The stacks come
 * from the stack segment instead (memory.c).
 */
#define PLATFORM_MAX_PROCESSES 12
#define PLATFORM_QUEUE_SIZE 64          /* room for a burst of keys an application is slow to take */
#define PLATFORM_STACK_SIZE (32 * 1024)

/* The Project's folders, through the folder calls MARS and RARS gained for asmOS (files.c). */
#define PLATFORM_FILE_SYSTEM (FS_LIST | FS_DELETE | FS_RENAME | FS_MAKE_DIR)
/* No pixel loops of the machine's own: os/blit.c's C ones. */
#define PLATFORM_BLIT 0
/* No room for a second screen: the window manager draws straight onto the screen, and the
 * wallpaper is a colour or a pattern, not a picture of the screen's size. */
#define PLATFORM_BACK_BUFFER 0
#define PLATFORM_WALLPAPER_PICTURE 0
/* DOOM needs more memory than the 4 MB, and so does the Video Player, which reads its whole
 * 2.5 MB video at once: neither is here. */
#define PLATFORM_DOOM 0
#define PLATFORM_VIDEO 0
/* and the applications with large buffers, such as the Assembly Editor's Step back, keep less */
#define PLATFORM_SMALL_MEMORY 1

/* A process switched by a call keeps its registers on its own stack: only where that is. */
typedef struct {
    void *sp;
} PlatformContext;

#endif
