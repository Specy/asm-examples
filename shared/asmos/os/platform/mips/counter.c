/*
 * The instructions the program has run. MIPS32's own counter, CP0's Count register, goes up once
 * every two instructions and comes round after 32 bits, every 8.6 billion, so this asks the
 * editor's MARS instead, which counts every instruction: syscall 1101 answers the count in $a0 and
 * $a1, the low and the high word, as the C library's clock() asks it.
 */

uint64_t platform_instructions(void) {
    register int v0 __asm__("$2") = 1101;
    register unsigned low __asm__("$4");
    register unsigned high __asm__("$5");
    __asm__ volatile("syscall" : "+r"(v0), "=r"(low), "=r"(high) : : "memory");
    return (uint64_t)high << 32 | low;
}
