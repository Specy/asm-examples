/*
 * The clock is syscall 30, the time in milliseconds since 1970 began, as a 64-bit number in two
 * registers, and sleep is syscall 32. A `long` is 32 bits here, so the kernel's clock counts from
 * asmOS's first look at it rather than from 1970. The instruction counter is the processor's own
 * (counter.c in the processor's folder).
 */
#include <sim.h>

static long long clock_start;

long platform_now_ms(void) {
    long long now = sim_time();
    if (!clock_start) clock_start = now - 1;
    return (long)(now - clock_start);
}

int64_t platform_time(void) { return sim_time() / 1000; }

void platform_sleep_ms(int ms) { sim_sleep(ms); }
