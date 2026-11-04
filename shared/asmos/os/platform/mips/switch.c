/*
 * Switching processes on MIPS: the C half of the exception handler, trap.mips.
 *
 * The timer is coprocessor 0's: Count ($9) goes up once every two instructions, and when it
 * reaches Compare ($11) the timer asks for an interrupt, Cause bit 15, until Compare is written
 * again. So each tick sets the next one, a turn's worth of Count on, as Linux's interval timer does
 * on x86. Count counts instructions, not time, and how many of them make 10 ms depends on how fast
 * the machine running MARS is. So asmOS measures it as it starts, as Linux on MIPS calibrates Count
 * against a clock it knows: it runs for a moment and compares Count with syscall 30's clock.
 * Status bit 15 lets the timer's interrupt through, or keeps it back while no application waits
 * for the processor. The three instructions for these are trap.mips's mips_ functions.
 *
 * trap.mips keeps a stopped process's registers in a frame on its stack and calls
 * platform_interrupt, which asks the kernel whether another process goes on (kernel_switch). If
 * one does, the stopped process's context keeps where its frame is, and the handler is given the
 * next one's frame to load. A process that has not run yet has a frame made to look the same.
 */
#include <sim.h>

/* trap.mips's frame: 16 bytes for a callee's arguments, then the pc, $1 to $31, hi, lo, cc0 and
 * the floating-point registers, 288 bytes in all. */
enum { FRAME = 288, FRAME_PC = 16, ARGUMENT_AREA = 16 };
/* Cause's exception code, shifted as it sits there, and the timer's bit in Cause and in Status. */
enum { EXCEPTION_CODE = 0x7c, CODE_INTERRUPT = 0, TIMER = 1 << 15 };
/* How long asmOS measures Count against the clock for, after running as long first, since an
 * emulator starts slower than it goes on: in CALIBRATE_WINDOWS parts, of which the fastest
 * counts, since a pause of the machine running MARS, its garbage collector's say, slows only the
 * part it falls in. And the work it measures it on: a loop of loads, adds and stores, as most of
 * what asmOS runs is. */
enum { CALIBRATE_MS = 100, CALIBRATE_WINDOWS = 5, CALIBRATE_STEPS = 2048 };
/* The fewest Counts a millisecond asmOS believes, whatever it measured: a machine stalled for the
 * whole calibration, its tab in the background say, would otherwise make a turn a few Counts long,
 * shorter than the handler, whose tick would then be due again before it returned, for ever. 500
 * makes a turn 5,000 Counts, 10,000 instructions, at the least: slower than any emulator runs. */
enum { MIN_COUNTS_PER_MS = 500 };

unsigned mips_count(void);
void mips_set_compare(unsigned when);
void mips_timer_enable(int on);

/* A new process's stack, as the handler leaves a stopped one's: a frame at the top, below the 16
 * bytes a caller keeps for `entry`'s arguments, every register in it 0 but the pc, `entry`, which
 * the handler's `eret` goes to. */
void platform_context_init(PlatformContext *context, void *stack, size_t size, void (*entry)(void)) {
    uintptr_t top = (((uintptr_t)stack + size) & ~(uintptr_t)7) - ARGUMENT_AREA;
    uint32_t *frame = (uint32_t *)(top - FRAME);
    memset(frame, 0, FRAME);
    frame[FRAME_PC / 4] = (uint32_t)(uintptr_t)entry;
    context->sp = frame;
}

static unsigned counts_per_ms = MIN_COUNTS_PER_MS, turn_counts;
static volatile unsigned calibrate_work[64];

/* Runs the calibration's loop for `ms` milliseconds from `start`, and answers when it stopped. */
static long calibrate_run(long start, long ms) {
    long now;
    do {
        for (int i = 0; i < CALIBRATE_STEPS; i++) calibrate_work[i & 63] += (unsigned)i;
        now = platform_now_ms();
    } while (now - start < ms);
    return now;
}

/* The timer stays quiet until platform_timer turns it on; first, how fast Count goes here. */
void platform_scheduler_start(void) {
    mips_timer_enable(0);
    long start = calibrate_run(platform_now_ms(), CALIBRATE_MS);
    unsigned fastest = 0;
    for (int i = 0; i < CALIBRATE_WINDOWS; i++) {
        unsigned first = mips_count();
        long now = calibrate_run(start, CALIBRATE_MS / CALIBRATE_WINDOWS);
        unsigned counts = (mips_count() - first) / (unsigned)(now - start);
        if (counts > fastest) fastest = counts;
        start = now;
    }
    counts_per_ms = fastest < MIN_COUNTS_PER_MS ? MIN_COUNTS_PER_MS : fastest;
    char line[160];
    if (fastest < MIN_COUNTS_PER_MS)
        snprintf(line, sizeof line, "asmOS: Count went up only %u times a millisecond while it was measured; "
                 "a turn is %u Counts, as at %u a millisecond.\n", fastest, counts_per_ms * TURN_MS, counts_per_ms);
    else
        snprintf(line, sizeof line, "asmOS: Count goes up %u times a millisecond here: a turn is %u of them.\n",
                 counts_per_ms, counts_per_ms * TURN_MS);
    platform_print(line);
}

/* A tick every `interval_ms`, or none for 0. */
void platform_timer(int interval_ms) {
    if (!interval_ms) {
        mips_timer_enable(0);
        return;
    }
    turn_counts = (unsigned)interval_ms * counts_per_ms;
    mips_set_compare(mips_count() + turn_counts);   /* which also withdraws a tick due */
    mips_timer_enable(1);
}

void platform_fault(unsigned cause, uintptr_t pc, uintptr_t address);

/* trap.mips's, for an interrupt or platform_trap's trap: answers the frame of the process that
 * goes on. */
void *platform_interrupt(unsigned cause, uintptr_t pc, void *frame) {
    int by_timer = (cause & EXCEPTION_CODE) == CODE_INTERRUPT;
    if (by_timer) {
        if (!(cause & TIMER)) platform_fault(cause, pc, 0);
        mips_set_compare(mips_count() + turn_counts);
    }
    PlatformContext *from, *to;
    if (!kernel_switch(by_timer, pc, &from, &to)) return frame;
    from->sp = frame;
    return to->sp;
}

/* trap.mips's, for any other exception: says what it was, where and in which process, and ends
 * asmOS, as Linux ends a program that faults on x86. */
void platform_fault(unsigned cause, uintptr_t pc, uintptr_t address) {
    const char *what;
    switch ((cause & EXCEPTION_CODE) >> 2) {
    case 0: what = "an interrupt asmOS did not turn on"; break;
    case 4: what = "a load from a misaligned address or one that is not there"; break;
    case 5: what = "a store to a misaligned address or one that is not there"; break;
    case 8: what = "a system call MARS refused, such as one asking for more memory than is left"; break;
    case 9: what = "a break"; break;
    case 10: what = "an instruction that does not exist"; break;
    case 12: what = "an overflow"; break;
    case 13: what = "a trap (compiled C's check for a division by zero)"; break;
    default: what = "an exception"; break;
    }
    char line[200];
    snprintf(line, sizeof line, "asmOS stopped: %s at 0x%08lx (Cause 0x%08x, BadVAddr 0x%08lx), in %s.\n",
             what, (unsigned long)pc, cause, (unsigned long)address, current->name);
    platform_print(line);
    sim_exit2(1);
}
