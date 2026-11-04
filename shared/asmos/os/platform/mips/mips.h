/*
 * asmOS on MIPS: one program on the editor's MARS, a 32-bit MIPS processor (MIPS32, little-endian)
 * with MARS's memory map and devices. What it shares with the RISC-V port, the screen, the keyboard
 * and the mouse, sound, the clock, memory and the files, is in os/platform/sim/; what is MIPS's
 * own is beside this File:
 *
 *   trap.mips   the exception handler every interrupt and exception goes to, at 0x80000180, in
 *               MIPS assembly: it keeps a stopped process's registers and loads the next one's
 *   switch.c    its C half: the timer, Count and Compare, measured against the clock as asmOS
 *               starts, which of their interrupts preempt a process, and a report of an exception
 *   counter.c   the instructions run, which the editor's MARS counts (syscall 1101)
 */
#ifndef ASMOS_MIPS_H
#define ASMOS_MIPS_H

#include "../sim/machine.h"

/* About asmOS's Processor line, the Terminal's uname -a, and About's lines on how asmOS runs. */
#define PLATFORM_MACHINE                                                                           \
    { "MIPS32, emulated by MARS", "mips (one program on the emulated MIPS machine)",               \
      "with its trap handler in MIPS assembly.", "One program on the emulated MIPS",               \
      "machine; its files are the Project's." }
/* What the boot screen says asmOS is written in. */
#define PLATFORM_LANGUAGES "C and MIPS assembly"

#endif
