/*
 * asmOS on x86: one ordinary Linux program on the emulated x86-64 machine. What it does with each
 * of the platform layer's parts is in the Files beside this one:
 *
 *   screen.c    the framebuffer /dev/fb0, mapped into memory; the console is standard output
 *   input.c     the keyboard and the USB tablet, as Linux input events from /dev/input
 *   time.c      Linux's clocks
 *   memory.c    the stacks, from the C library's heap
 *   switch.c    processes switched in a signal handler, preempted by an interval timer
 *   switch.asm  or, built cooperative, switched by a call
 *   sound.c     /dev/dsp and the MIDI port /dev/snd/midiC1D0
 *   blit.asm    the pixel loops, as `rep` instructions
 *
 * The files are the Project's folders, the program's working directory, through the POSIX calls
 * os/platform/files.c makes on every machine that has them.
 */
#ifndef ASMOS_X86_H
#define ASMOS_X86_H

/* About asmOS's Processor line, the Terminal's uname -a, and About's lines on how asmOS runs. */
#define PLATFORM_MACHINE                                                                           \
    { "x86-64, emulated by Blink", "x86_64 (a Linux program on the emulated x86 machine)",        \
      "with its pixel loops in x86 assembly.", "One Linux program on the emulated",                \
      "machine; its files are the Project's." }
/* What the boot screen says asmOS is written in. */
#define PLATFORM_LANGUAGES "C and x86 assembly"

/* Linux's interval timer interrupts a process anywhere, so asmOS preempts. Building with
 * -DASMOS_COOPERATIVE, or defining it here, tries what a machine without a timer does instead:
 * no timer, and switch.asm's switch by a call. */
/* #define ASMOS_COOPERATIVE */
#ifdef ASMOS_COOPERATIVE
#define PLATFORM_PREEMPTIVE 0
#else
#define PLATFORM_PREEMPTIVE 1
#endif

/* Memory: Linux gives the program as much as the machine has, so the kernel's tables are large. */
#define PLATFORM_MAX_PROCESSES 32
#define PLATFORM_QUEUE_SIZE 128         /* the messages that can wait for each process */
#define PLATFORM_STACK_SIZE (256 * 1024)
/* everything; -DPLATFORM_FILE_SYSTEM=0 tries a machine whose files are only read and written */
#ifndef PLATFORM_FILE_SYSTEM
#define PLATFORM_FILE_SYSTEM (FS_LIST | FS_DELETE | FS_RENAME | FS_MAKE_DIR)
#endif
/* blit.asm's `rep` loops; -DPLATFORM_BLIT=0 tries os/blit.c's C ones instead */
#ifndef PLATFORM_BLIT
#define PLATFORM_BLIT 1
#endif
/* a back buffer the window manager composes in, and a picture as the wallpaper;
 * -DPLATFORM_BACK_BUFFER=0 tries drawing straight onto the screen, as a machine short of memory would */
#ifndef PLATFORM_BACK_BUFFER
#define PLATFORM_BACK_BUFFER 1
#endif
#define PLATFORM_WALLPAPER_PICTURE PLATFORM_BACK_BUFFER
/* room for DOOM and the Video Player */
#define PLATFORM_DOOM 1
#define PLATFORM_VIDEO 1

#if PLATFORM_PREEMPTIVE
/* What the signal frame holds of a process: its general-purpose registers, in Linux's order, and
 * the 512 bytes FXSAVE writes of the x87 and SSE state. A process that has not run yet has only
 * where its stack starts and where it starts. */
enum { PLATFORM_GREGS = 23 };
typedef struct {
    uint64_t gregs[PLATFORM_GREGS];
    uint8_t fx[512] __attribute__((aligned(16)));
    int started;
} PlatformContext;
#else
/* A process switched by a call keeps its registers on its own stack: only where that is. */
typedef struct {
    void *sp;
} PlatformContext;
#endif

#endif
