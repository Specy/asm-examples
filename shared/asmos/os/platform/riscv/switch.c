/*
 * Switching processes on RISC-V: the C half of the trap handler, trap.riscv.
 *
 * The timer is RARS's Timer Tool: `time`, at 0xFFFF0018, counts the milliseconds the program has
 * run, and once it reaches `timecmp`, at 0xFFFF0020, the timer asks for one interrupt, which
 * re-arms it only when timecmp is written again. So each tick sets the next one, 10 ms on, as
 * Linux's interval timer does on x86. <sim.h> reads and writes the two (sim_timer_now and
 * sim_timer_set).
 *
 * trap.riscv keeps a stopped process's registers in a frame on its stack and calls
 * platform_interrupt, which asks the kernel whether another process goes on (kernel_switch). If
 * one does, the stopped process's context keeps where its frame is, and the handler is given the
 * next one's frame to load. A process that has not run yet has a frame made to look the same.
 */
#include <sim.h>

/* trap.riscv's frame: the pc in the first word, x1 to x31 at 4 times their number, f0 to f31,
 * fcsr at 384, 400 bytes in all. */
enum { FRAME = 400, FRAME_PC = 0 };
/* What ucause holds for the two interrupts asmOS asks for, the top bit aside. */
enum { CAUSE_SOFTWARE = 0, CAUSE_TIMER = 4 };

/* A new process's stack, as the handler leaves a stopped one's: a frame at the top, every register
 * in it 0 but the pc, `entry`, which the handler's `uret` goes to. */
void platform_context_init(PlatformContext *context, void *stack, size_t size, void (*entry)(void)) {
    uintptr_t top = ((uintptr_t)stack + size) & ~(uintptr_t)15;
    uint32_t *frame = (uint32_t *)(top - FRAME);
    memset(frame, 0, FRAME);
    frame[FRAME_PC / 4] = (uint32_t)(uintptr_t)entry;
    context->sp = frame;
}

static int timer_interval;

/* A tick every `interval_ms`, or none for 0: a timecmp of all ones is never reached. */
void platform_timer(int interval_ms) {
    timer_interval = interval_ms;
    sim_timer_set(interval_ms ? sim_timer_now() + (unsigned)interval_ms : ~0ull);
}

/* trap.riscv's, for an interrupt: answers the frame of the process that goes on. */
void *platform_interrupt(unsigned cause, uintptr_t pc, void *frame) {
    int by_timer = (cause & 0xff) == CAUSE_TIMER;
    if (by_timer && timer_interval) sim_timer_set(sim_timer_now() + (unsigned)timer_interval);
    PlatformContext *from, *to;
    if (!kernel_switch(by_timer, pc, &from, &to)) return frame;
    from->sp = frame;
    return to->sp;
}

/* trap.riscv's, for an exception: says what it was, where and in which process, and ends asmOS,
 * as Linux ends a program that faults on x86. */
void platform_fault(unsigned cause, uintptr_t pc, uintptr_t value) {
    static const char *const what[] = {
        "a jump to a misaligned address", "an instruction fetched from memory that is not there",
        "an instruction that does not exist", "a breakpoint", "a misaligned load",
        "a load from memory that is not there", "a misaligned store",
        "a store to memory that is not there", "a failed environment call",
    };
    char line[200];
    snprintf(line, sizeof line, "asmOS stopped: %s at 0x%08lx (ucause %u, utval 0x%08lx), in %s.\n",
             cause < sizeof what / sizeof *what ? what[cause] : "an exception", (unsigned long)pc, cause,
             (unsigned long)value, current->name);
    platform_print(line);
    sim_exit2(1);
}
