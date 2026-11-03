/*
 * Switching processes on x86.
 *
 * Preemptive, the way asmOS runs here: in a signal handler. Linux stops the program wherever it is
 * to deliver a signal and hands the handler the signal frame, the registers it stopped it with:
 * the general-purpose ones and the 512 bytes of x87 and SSE state. The return from the handler
 * loads them back. So the handler keeps the stopped process's registers and writes the next
 * process's into the frame, and the return resumes that one where it was stopped. The interval
 * timer's SIGALRM, every 10 ms while applications are ready, takes the processor from a process
 * that does not give it up; a process that gives it up sends itself SIGUSR2.
 *
 * Built with ASMOS_COOPERATIVE (x86.h): no timer, and switch.asm switches in a call.
 */
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>

#if PLATFORM_PREEMPTIVE

/* The general-purpose registers in the signal frame, in Linux's order for x86-64. */
enum { R8, R9, R10, R11, R12, R13, R14, R15, RDI, RSI, RBP, RBX, RDX, RAX, RCX, RSP, RIP, EFL };

/* The signal frame's context, as Linux lays it out (sys/ucontext.h, which the headers here do not
 * have): what the handler reads the stopped process from and writes the next one into. */
typedef struct {
    uint64_t gregs[PLATFORM_GREGS];
    void *fpregs;          /* 512 bytes in FXSAVE's layout */
    uint64_t reserved[8];
} MachineContext;
typedef struct {
    uint64_t flags;
    void *link;
    struct {
        void *sp;
        int flags;
        size_t size;
    } stack;
    MachineContext machine;
} SignalContext;

void platform_context_init(PlatformContext *context, void *stack, size_t size, void (*entry)(void)) {
    /* a stack as a call leaves it: 16-aligned, less the return address */
    context->gregs[RSP] = ((uint64_t)((uint8_t *)stack + size) & ~(uint64_t)15) - 8;
    context->gregs[RBP] = 0;
    context->gregs[RIP] = (uint64_t)entry;
    context->started = 0;
}

/* SIGUSR2 is a process giving the processor up; SIGALRM is the timer taking it. */
static void on_switch_signal(int signal, siginfo_t *info, void *frame) {
    (void)info;
    SignalContext *context = frame;
    uint64_t *gregs = context->machine.gregs;
    PlatformContext *from, *to;
    if (!kernel_switch(signal == SIGALRM, gregs[RIP], &from, &to)) return;
    memcpy(from->gregs, gregs, sizeof from->gregs);
    if (context->machine.fpregs) memcpy(from->fx, context->machine.fpregs, sizeof from->fx);
    from->started = 1;
    if (to->started) {
        for (int r = R8; r <= EFL; r++) gregs[r] = to->gregs[r];
        if (context->machine.fpregs) memcpy(context->machine.fpregs, to->fx, sizeof to->fx);
    } else {
        /* its first turn: the rest of the registers do not matter yet */
        gregs[RSP] = to->gregs[RSP];
        gregs[RBP] = to->gregs[RBP];
        gregs[RIP] = to->gregs[RIP];
        to->started = 1;
    }
}

void platform_scheduler_start(void) {
    struct sigaction action;
    memset(&action, 0, sizeof action);
    action.sa_sigaction = on_switch_signal;
    action.sa_flags = SA_SIGINFO | SA_RESTART;
    /* neither signal interrupts the handler */
    sigemptyset(&action.sa_mask);
    sigaddset(&action.sa_mask, SIGALRM);
    sigaddset(&action.sa_mask, SIGUSR2);
    sigaction(SIGALRM, &action, NULL);
    sigaction(SIGUSR2, &action, NULL);
}

void platform_timer(int interval_ms) {
    struct itimerval interval = { { 0, interval_ms * 1000 }, { 0, interval_ms * 1000 } };
    setitimer(ITIMER_REAL, &interval, NULL);
}

void platform_trap(void) { kill(getpid(), SIGUSR2); }

#else

/* switch.asm's platform_switch keeps a process's registers on its stack, below its return
 * address, and its stack pointer in the context. A new process's stack is made to look the same,
 * so that the switch's `ret` goes to `entry`: */
void platform_context_init(PlatformContext *context, void *stack, size_t size, void (*entry)(void)) {
    uint64_t *top = (uint64_t *)((uintptr_t)((uint8_t *)stack + size) & ~(uintptr_t)15);
    *--top = 0;                       /* the return address `entry` would have, which never returns */
    *--top = (uint64_t)entry;         /* where the switch returns to */
    for (int i = 0; i < 6; i++) *--top = 0;   /* rbp, rbx, r12, r13, r14 and r15 */
    *--top = 0x037f00001f80;          /* the x87 control word and MXCSR as Linux starts a program */
    context->sp = top;
}

#endif
