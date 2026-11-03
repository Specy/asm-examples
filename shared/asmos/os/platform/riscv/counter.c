/*
 * The instructions the program has run: every RISC-V processor counts them in instret, a 64-bit
 * counter read in two halves on a 32-bit one (rdinstret and rdinstreth). The high half is read
 * again until it stays the same, in case the low half overflowed between the two reads.
 */

uint64_t platform_instructions(void) {
    uint32_t high, low, again;
    do {
        __asm__ volatile("rdinstreth %0\n\trdinstret %1\n\trdinstreth %2" : "=r"(high), "=r"(low), "=r"(again));
    } while (high != again);
    return (uint64_t)high << 32 | low;
}
