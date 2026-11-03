/*
 * Memory on x86: Linux gives the program pages as it asks for them, up to what the machine has,
 * so the stacks come from the C library's heap like any other memory, and nothing limits the heap
 * that the kernel need check.
 */
#include <stdint.h>
#include <stdlib.h>

void *platform_stack_alloc(size_t size) { return malloc(size); }
void platform_stack_free(void *stack) { free(stack); }
size_t platform_memory_left(void) { return SIZE_MAX; }
