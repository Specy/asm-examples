/*
 * Switching processes on RISC-V, by a call. RARS has no timer asmOS uses, so a process gives the
 * processor up itself, calling platform_switch from the kernel as any function is called. The
 * calling convention (the RISC-V psABI's ilp32d) lets a function lose every register but the
 * saved ones: ra, sp, s0 to s11 and the floating-point fs0 to fs11. So those are all the switch
 * keeps. It stores them on the process's own stack, the stack pointer into its context, then takes
 * the next process's stack pointer and loads the same registers back from there: its `ret` returns
 * into the other process, from the call that process once made. A process that has not run yet
 * has a stack made to look the same (platform_context_init), so the `ret` starts it.
 *
 * fcsr, the floating-point rounding mode and flags, stays as it is: asmOS never changes the mode.
 */

/* The frame the switch keeps on a stack: ra, s0 to s11, a word of padding and fs0 to fs11, 160
 * bytes so the stack stays 16-byte aligned. */
enum { SWITCH_FRAME = 160, SWITCH_RA = 0 };

/* void platform_switch(PlatformContext *from, PlatformContext *to): a0 = from, a1 = to, and a
 * PlatformContext is the one stack pointer. */
__asm__(
    "    .text\n"
    "    .globl platform_switch\n"
    "    .p2align 2\n"
    "    .type platform_switch,@function\n"
    "platform_switch:\n"
    "    addi sp, sp, -160\n"
    "    sw ra, 0(sp)\n"
    "    sw s0, 4(sp)\n"
    "    sw s1, 8(sp)\n"
    "    sw s2, 12(sp)\n"
    "    sw s3, 16(sp)\n"
    "    sw s4, 20(sp)\n"
    "    sw s5, 24(sp)\n"
    "    sw s6, 28(sp)\n"
    "    sw s7, 32(sp)\n"
    "    sw s8, 36(sp)\n"
    "    sw s9, 40(sp)\n"
    "    sw s10, 44(sp)\n"
    "    sw s11, 48(sp)\n"
    "    fsd fs0, 64(sp)\n"
    "    fsd fs1, 72(sp)\n"
    "    fsd fs2, 80(sp)\n"
    "    fsd fs3, 88(sp)\n"
    "    fsd fs4, 96(sp)\n"
    "    fsd fs5, 104(sp)\n"
    "    fsd fs6, 112(sp)\n"
    "    fsd fs7, 120(sp)\n"
    "    fsd fs8, 128(sp)\n"
    "    fsd fs9, 136(sp)\n"
    "    fsd fs10, 144(sp)\n"
    "    fsd fs11, 152(sp)\n"
    "    sw sp, 0(a0)\n"          /* from->sp */
    "    lw sp, 0(a1)\n"          /* to->sp: the other process's stack from here on */
    "    lw ra, 0(sp)\n"
    "    lw s0, 4(sp)\n"
    "    lw s1, 8(sp)\n"
    "    lw s2, 12(sp)\n"
    "    lw s3, 16(sp)\n"
    "    lw s4, 20(sp)\n"
    "    lw s5, 24(sp)\n"
    "    lw s6, 28(sp)\n"
    "    lw s7, 32(sp)\n"
    "    lw s8, 36(sp)\n"
    "    lw s9, 40(sp)\n"
    "    lw s10, 44(sp)\n"
    "    lw s11, 48(sp)\n"
    "    fld fs0, 64(sp)\n"
    "    fld fs1, 72(sp)\n"
    "    fld fs2, 80(sp)\n"
    "    fld fs3, 88(sp)\n"
    "    fld fs4, 96(sp)\n"
    "    fld fs5, 104(sp)\n"
    "    fld fs6, 112(sp)\n"
    "    fld fs7, 120(sp)\n"
    "    fld fs8, 128(sp)\n"
    "    fld fs9, 136(sp)\n"
    "    fld fs10, 144(sp)\n"
    "    fld fs11, 152(sp)\n"
    "    addi sp, sp, 160\n"
    "    ret\n");

/* A new process's stack, as the switch leaves one: its frame at the top, every register in it 0
 * but ra, which the switch's `ret` goes to, `entry`. */
void platform_context_init(PlatformContext *context, void *stack, size_t size, void (*entry)(void)) {
    uintptr_t top = ((uintptr_t)stack + size) & ~(uintptr_t)15;
    uint32_t *frame = (uint32_t *)(top - SWITCH_FRAME);
    memset(frame, 0, SWITCH_FRAME);
    frame[SWITCH_RA] = (uint32_t)(uintptr_t)entry;
    context->sp = frame;
}
