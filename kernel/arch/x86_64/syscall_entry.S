
.global syscall_entry
.type syscall_entry, @function
syscall_entry:
    // Syscall number is in rdi
    // Arguments are stored in rsi, rdx, r10, r8, r9
    // (r10 takes rcx's palce as the 3rd argument, since syscall uses rcx)
    // rcx stores the user-mode instruction pointer, and r11 holds rflags

    // Interrupts are disabled on syscall entry, so we don't end up interrupted
    // with a mismatched code segment and gs base
    swapgs
    //sti

    // The user stack pointer isn't saved automatically
    mov %rsp, %rax

    // It is safe to just load the top of the stack as the pointer
    // because whenever user code is running in this thread it
    // should be completely empty.

    // Load current thread pointer from this_cpu->current_thread
    // rbx and r12-15 are callee preserved, so we cannot modify them without a stack to save to.
    // With the other registers being used for arguments and syscall/sysret bookkeeping the
    // only available option is rsp
    movq %gs:0, %rsp
    // Check offsets if object or thread structs change
    movq 0x78(%rsp), %rsp // Get kernel stack from thread control block

    push %rax
    push %rcx
    push %r11

    mov %r10, %rcx

    push %rbx
    // rcx used by context switch
    push %rdx
    push %rdi
    push %rsi
    push %r8
    push %r9
    push %r10
    // r11 used by context switch
    push %r12
    push %r13
    push %r14
    push %r15

    push %rbp
    mov %rsp, %rbp

    // Now following System V ABI and we have a kernel stack, enter C
    .extern syscall_handler
    .type syscall_handler, @function
    call syscall_handler
    // %rax holds the return value

    pop %rbp

    pop %r15
    pop %r14
    pop %r13
    pop %r12
    // r11 used by context switch
    pop %r10
    pop %r9
    pop %r8
    pop %rsi
    pop %rdi
    pop %rdx
    // rcx used by context switch
    pop %rbx

    // Avoid being interrupted while having the user GS base in kernel code,
    // Since interrupt handlers use CS to determine whether to use swapgs again
    // Interrupts will be enabled during the return to userspace
    cli

    swapgs

    pop %r11
    or $(1 << 9), %r11 // Set interrupt flag in saved rflags
    pop %rcx
    pop %rsp
    sysretq
