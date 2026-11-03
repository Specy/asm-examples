/*
 * Linux's clocks. CLOCK_MONOTONIC only goes forward. CLOCK_PROCESS_CPUTIME_ID is the processor
 * time the program has used, which the emulator counts in instructions, as a processor of
 * 100 MHz would: ten nanoseconds an instruction.
 */
#include <time.h>

long platform_now_ms(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return now.tv_sec * 1000L + now.tv_nsec / 1000000;
}

uint64_t platform_instructions(void) {
    struct timespec now;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &now);
    return (uint64_t)now.tv_sec * 100000000 + (uint64_t)now.tv_nsec / 10;
}

int64_t platform_time(void) { return time(NULL); }

void platform_sleep_ms(int ms) {
    struct timespec pause = { ms / 1000, ms % 1000 * 1000000L };
    while (nanosleep(&pause, &pause) != 0) {}
}
