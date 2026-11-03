/*
 * Switching processes on MIPS, by a call. MARS has no timer asmOS uses, so a process gives the
 * processor up itself, calling platform_switch from the kernel as any function is called. The
 * calling convention (o32, with 64-bit floating-point registers in pairs) lets a function lose
 * every register but the saved ones: $s0 to $s7 ($16 to $23), $fp ($30), $ra ($31), $sp ($29) and
 * the floating-point $f20 to $f31, six doubles. So those are all the switch keeps. It stores them
 * on the process's own stack, the stack pointer into its context, then takes the next process's
 * stack pointer and loads the same registers back from there: its `jr $ra` returns into the other
 * process, from the call that process once made. A process that has not run yet has a stack made
 * to look the same (platform_context_init), so the jump starts it.
 *
 * The registers are written by number, as the compiler writes them. The instruction after the
 * jump is its delay slot, a nop, which the editor's MARS does not run.
 */

/* The frame the switch keeps on a stack: $ra, $fp, $s0 to $s7, then $f20 to $f30 as doubles,
 * 88 bytes, a multiple of 8 so the doubles stay aligned. */
enum { SWITCH_FRAME = 88, SWITCH_RA = 0, ARGUMENT_AREA = 16 };

/* void platform_switch(PlatformContext *from, PlatformContext *to): $a0 ($4) = from, $a1 ($5) = to,
 * and a PlatformContext is the one stack pointer. */
__asm__(
    "    .text\n"
    "    .globl platform_switch\n"
    "    .p2align 2\n"
    "    .type platform_switch,@function\n"
    "    .set noreorder\n"
    "platform_switch:\n"
    "    addiu $29, $29, -88\n"
    "    sw $31, 0($29)\n"
    "    sw $30, 4($29)\n"
    "    sw $16, 8($29)\n"
    "    sw $17, 12($29)\n"
    "    sw $18, 16($29)\n"
    "    sw $19, 20($29)\n"
    "    sw $20, 24($29)\n"
    "    sw $21, 28($29)\n"
    "    sw $22, 32($29)\n"
    "    sw $23, 36($29)\n"
    "    sdc1 $f20, 40($29)\n"
    "    sdc1 $f22, 48($29)\n"
    "    sdc1 $f24, 56($29)\n"
    "    sdc1 $f26, 64($29)\n"
    "    sdc1 $f28, 72($29)\n"
    "    sdc1 $f30, 80($29)\n"
    "    sw $29, 0($4)\n"         /* from->sp */
    "    lw $29, 0($5)\n"         /* to->sp: the other process's stack from here on */
    "    lw $31, 0($29)\n"
    "    lw $30, 4($29)\n"
    "    lw $16, 8($29)\n"
    "    lw $17, 12($29)\n"
    "    lw $18, 16($29)\n"
    "    lw $19, 20($29)\n"
    "    lw $20, 24($29)\n"
    "    lw $21, 28($29)\n"
    "    lw $22, 32($29)\n"
    "    lw $23, 36($29)\n"
    "    ldc1 $f20, 40($29)\n"
    "    ldc1 $f22, 48($29)\n"
    "    ldc1 $f24, 56($29)\n"
    "    ldc1 $f26, 64($29)\n"
    "    ldc1 $f28, 72($29)\n"
    "    ldc1 $f30, 80($29)\n"
    "    addiu $29, $29, 88\n"
    "    jr $31\n"
    "    nop\n"
    "    .set reorder\n");

/* A new process's stack, as the switch leaves one: its frame at the top, every register in it 0
 * but $ra, which the switch's jump goes to, `entry`. Above the frame are the 16 bytes a caller
 * keeps for the arguments its callee may store, which `entry` has the right to. */
void platform_context_init(PlatformContext *context, void *stack, size_t size, void (*entry)(void)) {
    uintptr_t top = (((uintptr_t)stack + size) & ~(uintptr_t)7) - ARGUMENT_AREA;
    uint32_t *frame = (uint32_t *)(top - SWITCH_FRAME);
    memset(frame, 0, SWITCH_FRAME);
    frame[SWITCH_RA] = (uint32_t)(uintptr_t)entry;
    context->sp = frame;
}
