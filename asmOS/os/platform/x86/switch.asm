; The cooperative switch, for asmOS built with ASMOS_COOPERATIVE (x86.h): the way a machine with no
; timer to preempt a process switches, tried on x86. A process gives the processor up by calling
; this from the kernel, as any function is called, so it only has to keep what the System V
; calling convention says a function leaves as it found: rbx, rbp and r12 to r15, the stack
; pointer, and the control bits of MXCSR and of the x87 unit. Everything else the caller expects
; to lose. They go on the process's own stack, and the stack pointer into its context; then the
; next process's stack pointer comes back, and the same steps backwards resume it. A process that
; has not run yet has a stack made to look the same (switch.c), so the `ret` starts it.
;
; Only a build with ASMOS_COOPERATIVE calls it; the preemptive one leaves it out of the program.

    default rel
    section .text

    global platform_switch

; void platform_switch(PlatformContext *from, PlatformContext *to): rdi = from, rsi = to, and a
; PlatformContext is the one stack pointer.
platform_switch:
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp, 8
    stmxcsr [rsp]               ; MXCSR in the low 4 bytes
    fnstcw [rsp + 4]            ; the x87 control word in the next 2
    mov [rdi], rsp              ; from->sp
    mov rsp, [rsi]              ; to->sp: the other process's stack from here on
    ldmxcsr [rsp]
    fldcw [rsp + 4]
    add rsp, 8
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret
