/*
 * asmOS on RISC-V: one program on the editor's RARS, a 32-bit RISC-V processor (RV32IMFD) with
 * RARS's memory map and devices. What it shares with the MIPS port, the screen, the keyboard and
 * the mouse, sound, the clock, memory and the files, is in os/platform/sim/; what is RISC-V's own
 * is beside this File:
 *
 *   trap.riscv  the trap handler every interrupt and exception goes to, in RISC-V assembly: it
 *               keeps a stopped process's registers and loads the next one's
 *   switch.c    its C half: the timer, which of RARS's Timer Tool's interrupts preempt a process,
 *               and a report of an exception
 *   counter.c   the instructions run, from the instret counter every RISC-V processor has
 */
#ifndef ASMOS_RISCV_H
#define ASMOS_RISCV_H

#include "../sim/machine.h"

/* About asmOS's Processor line, the Terminal's uname -a, and About's lines on how asmOS runs. */
#define PLATFORM_MACHINE                                                                           \
    { "RV32IMFD, emulated by RARS", "riscv32 (one program on the emulated RISC-V machine)",        \
      "with its trap handler in RISC-V assembly.", "One program on the emulated RISC-V",           \
      "machine; its files are the Project's." }
/* What the boot screen says asmOS is written in. */
#define PLATFORM_LANGUAGES "C and RISC-V assembly"

#endif
