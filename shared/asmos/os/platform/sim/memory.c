/*
 * Memory on MARS and RARS. Static data and the heap share the data segment, from 0x10010000 to
 * the end the editor's MARS and RARS give it, <sim.h>'s SIM_DATA_SEGMENT_END: 64 MB, where MARS
 * and RARS themselves stop at 0x10400000. The heap grows from the end of static data, and the
 * machine ends the program that asks it for more than that, so the kernel checks what is left as
 * it starts, and before it lets the heap grow (kernel.c's heap_alloc). The stack
 * segment is another 4 MB, from 0x7ffffffc down to 0x7fc00000, all of it ordinary memory however
 * far it is from the stack pointer, and the system's own stack, `main`'s, uses only its top. So
 * each process's stack is a slot at the segment's far end, and costs the heap nothing.
 */
#include <sim.h>

enum { STACK_SEGMENT_END = 0x7fc00000 };

/* Where asmOS takes the heap to end: the data segment's end, or lower, -DASMOS_HEAP_END=0x10600000
 * say, to try asmOS short of memory, as tools/harness/scripts/sim-low-memory.json does. */
#ifndef ASMOS_HEAP_END
#define ASMOS_HEAP_END SIM_DATA_SEGMENT_END
#endif

/* Which slots a process has, one for each process the kernel can have. */
static unsigned char stack_slots[PLATFORM_MAX_PROCESSES];

void *platform_stack_alloc(size_t size) {
    if (size > PLATFORM_STACK_SIZE) return NULL;
    for (int i = 0; i < PLATFORM_MAX_PROCESSES; i++)
        if (!stack_slots[i]) {
            stack_slots[i] = 1;
            return (void *)(uintptr_t)(STACK_SEGMENT_END + (uintptr_t)i * PLATFORM_STACK_SIZE);
        }
    return NULL;
}

void platform_stack_free(void *stack) {
    uintptr_t slot = ((uintptr_t)stack - STACK_SEGMENT_END) / PLATFORM_STACK_SIZE;
    if (slot < PLATFORM_MAX_PROCESSES) stack_slots[slot] = 0;
}

/* What the heap can still grow by: up to the data segment's end from where it ends now, which
 * service 9 answers when asked for no bytes. */
size_t platform_memory_left(void) {
    uintptr_t end = (uintptr_t)sim_sbrk(0);
    return end < ASMOS_HEAP_END ? ASMOS_HEAP_END - end : 0;
}
