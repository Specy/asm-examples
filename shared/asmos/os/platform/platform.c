/*
 * The machine's half of the platform layer, chosen as platform.h chooses its header. os/main.c
 * includes this after the kernel, which these Files call back (kernel_switch) and whose helpers
 * they use.
 */
#if defined(__x86_64__)
#include "x86/screen.c"
#include "x86/input.c"
#include "x86/time.c"
#include "x86/memory.c"
#include "x86/switch.c"
#include "x86/sound.c"
#elif defined(__riscv) || defined(__mips__)
#include "sim/screen.c"
#include "sim/input.c"
#include "sim/time.c"
#include "sim/memory.c"
#include "sim/sound.c"
#if defined(__riscv)
#include "riscv/switch.c"
#include "riscv/counter.c"
#else
#include "mips/switch.c"
#include "mips/counter.c"
#endif
#endif
#include "files.c"
