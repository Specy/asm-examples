/*
 * The platform layer: everything asmOS asks of the machine it runs on. The rest of asmOS, the
 * kernel, the window manager and the applications, is the same C on every machine; each machine
 * has a folder here, os/platform/<machine>/, that does these few things its own way:
 *
 *   x86/    a Linux program: the framebuffer /dev/fb0, the keyboard and the tablet in /dev/input,
 *           sound on /dev/dsp and a MIDI port, a timer signal whose handler switches processes,
 *           and the Project's folders as Linux files.
 *   riscv/  one program on RARS, and mips/ one on MARS: a bitmap display in memory, the editor's
 *           Input controller, no sound, processes that give the processor up themselves, and the
 *           Project's folders through MARS's and RARS's file and folder calls. What the two share,
 *           all but the processor's own switch and counter, is in sim/.
 *
 * files.c, the Project's files and folders through the POSIX calls, serves every machine.
 *
 * The compiler says which machine it compiles for, and this header includes that machine's own,
 * which sets the PLATFORM_ constants below and the PlatformContext a process's registers are
 * kept in while it does not run. Only the kernel's half, os/main.c, calls the platform; the
 * applications ask the kernel, through os.h.
 */
#ifndef ASMOS_PLATFORM_H
#define ASMOS_PLATFORM_H

#include <stddef.h>
#include <stdint.h>
#include "../os.h"

#if defined(__x86_64__)
#include "x86/x86.h"
#elif defined(__riscv) && __riscv_xlen == 32
#include "riscv/riscv.h"
#elif defined(__mips__)
#include "mips/mips.h"
#else
#error "asmOS has no platform layer for this machine yet"
#endif

/*
 * What the machine's header sets:
 *
 *   PLATFORM_MACHINE      the machine in words, os_machine's (os.h)
 *   PLATFORM_LANGUAGES    what the boot screen says asmOS is written in
 *   PLATFORM_PREEMPTIVE   1 when a timer can take the processor from a process, 0 when every
 *                         process must give it up itself: then the kernel switches in a plain
 *                         function call, platform_switch, and os_yield_point counts
 *   PLATFORM_MAX_PROCESSES  how many processes there can be, the system's included
 *   PLATFORM_QUEUE_SIZE   how many messages can wait for each process: with the first, most of
 *                         the kernel's own memory
 *   PLATFORM_STACK_SIZE   the bytes of each process's stack
 *   PLATFORM_FILE_SYSTEM  what the file system can do beyond whole files: FS_ bits of os.h
 *   PLATFORM_BLIT         1 when the machine has its own fill_rect32, copy_rect32 and move_rect32
 *                         (os.h); 0 for the C ones of os/blit.c
 *   PLATFORM_BACK_BUFFER  1 when there is memory for a back buffer the size of the screen, which the
 *                         window manager composes in; 0 when it draws straight onto the screen
 *   PLATFORM_WALLPAPER_PICTURE  1 when a picture the size of the screen fits as the wallpaper
 *   PLATFORM_DOOM, PLATFORM_VIDEO  1 when DOOM, and the Video Player, have the memory they need;
 *                         an application the machine leaves out is not in its APPS (apps.c)
 *   PLATFORM_SMALL_MEMORY 1 where all of static data and the heap is a few megabytes, as on MARS
 *                         and RARS: the applications that keep large buffers keep smaller ones
 */

/* ------------------------------------------------------------------ the screen and the console */

/* The screen as a picture of 0x00RRGGBB pixels, `stride` pixels from one row to the next: 0, or
 * -1 when there is none. */
int platform_screen_open(uint32_t **pixels, int *width, int *height, int *stride);
/* Shows what was drawn in a rectangle of that picture: nothing to do where the picture is the
 * screen itself. */
void platform_screen_present(int x, int y, int w, int h);
/* Text for whoever started asmOS: the program's own output, not the screen. */
void platform_print(const char *text);

/* ------------------------------------------------------------------ the keyboard and the pointer */

enum {
    INPUT_KEY_DOWN, INPUT_KEY_UP, INPUT_KEY_REPEAT,   /* `code` is a KEY_ code, keys.h */
    INPUT_BUTTON_DOWN, INPUT_BUTTON_UP,               /* `code` is a BUTTON_ of os.h */
    INPUT_MOVE,
    INPUT_WHEEL                                       /* `code` is notches, + away from the user */
};
typedef struct {
    int kind;
    int code;
    int x, y;           /* where the pointer is, in screen pixels */
} InputEvent;

/* Finds the keyboard and the pointer: 1 when both are there. */
int platform_input_open(void);
/* Waits up to `timeout_ms` (-1: for ever) for input, then answers how many events it put in
 * `events`, at most `max`, and 0 once nothing more has arrived. */
int platform_input_read(InputEvent *events, int max, int timeout_ms);

/* ------------------------------------------------------------------ time */

/* Milliseconds by a clock that only goes forward, from no particular moment. */
long platform_now_ms(void);
/* The instructions the program has run, as far as the machine counts them. */
uint64_t platform_instructions(void);
/* Seconds since 1970 began, in UTC, by the machine's calendar clock. */
int64_t platform_time(void);
void platform_sleep_ms(int ms);

/* ------------------------------------------------------------------ memory */

/* A process's stack, from wherever the machine keeps them: NULL when there is no room. */
void *platform_stack_alloc(size_t size);
void platform_stack_free(void *stack);
/* The bytes the heap can still grow by, which the kernel checks its needs against as it starts:
 * SIZE_MAX where only the machine's own memory limits it. A machine may end the program, rather
 * than fail, when the heap would grow past its limit, as MARS and RARS do. */
size_t platform_memory_left(void);

/* ------------------------------------------------------------------ switching processes */

/* Prepares the context of a new process, so that its first turn calls `entry` on the stack of
 * `size` bytes at `stack`. */
void platform_context_init(PlatformContext *context, void *stack, size_t size, void (*entry)(void));

#if PLATFORM_PREEMPTIVE
/*
 * A preemptive machine switches in a trap handler, the platform's: the timer's tick, or a process
 * giving the processor up with platform_trap, stops the process running, the handler asks the
 * kernel which one runs next (kernel_switch), keeps the stopped one's registers in its context
 * and loads the next one's, and the return from the trap resumes that one.
 */
void platform_scheduler_start(void);  /* installs the trap handler */
void platform_timer(int interval_ms); /* a tick every so many milliseconds, or none for 0 */
void platform_trap(void);             /* the switch, from a process that gives the processor up */
/* The kernel's half, called by the handler: 1 when the processor goes from the process of
 * context `*from` to the one of `*to`, 0 when the stopped process goes on. `by_timer` says the
 * timer stopped it, at `pc`. */
int kernel_switch(int by_timer, uintptr_t pc, PlatformContext **from, PlatformContext **to);
#else
/* A cooperative machine switches in a call: keeps the caller's registers in `from` and goes on
 * where the process of `to` stopped, or at its entry. The caller returns from it when the
 * kernel switches back. */
void platform_switch(PlatformContext *from, PlatformContext *to);
#endif

/* ------------------------------------------------------------------ sound, each optional */

/* Sampled sound, 16-bit mono at `rate` a second: 0, or -1 when the machine has none. */
int platform_pcm_open(int rate);
int platform_pcm_space(void);         /* samples that fit without waiting */
int platform_pcm_write(const int16_t *samples, int count);   /* answers how many it took */
void platform_pcm_close(void);        /* what is still buffered is dropped */
/* A General MIDI synthesizer's port, raw MIDI bytes: 0, or -1 when there is none. */
int platform_midi_open(void);
void platform_midi_write(const unsigned char *bytes, int length);

/* ------------------------------------------------------------------ files and folders */

/* A path is relative to the Project, "pictures/sunset.bmp". Files are read and written whole. */
long platform_file_size(const char *path);                   /* -1 when no file is there */
long platform_file_read(const char *path, void *data, long size);   /* bytes read, -1 */
int platform_file_write(const char *path, const void *data, long size);   /* 0, or -1 */
/* 1 for a file, 2 for a folder, 0 for nothing; `size` gets a file's size. */
int platform_stat(const char *path, long *size);
/* What PLATFORM_FILE_SYSTEM may leave out answers -1: listing a folder (without its hidden
 * names, which begin with a dot, in no order), deleting a file or an empty folder, renaming
 * and making a folder. */
int platform_list_dir(const char *path, DirEntry *entries, int max);
int platform_delete(const char *path);
int platform_rename(const char *from, const char *to);
int platform_make_dir(const char *path);

#endif
