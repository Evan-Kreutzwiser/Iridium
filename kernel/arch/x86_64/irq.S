

.macro isr index
    .global _isr\index
    .type _isr\index, @function
    _isr\index:
    pushq $0x0; # Blank error code
    pushq $\index;
    jmp isr_common
.endm

.macro isr_error index
    .global _isr\index
    .type _isr\index, @function
    _isr\index:
    pushq $\index
    jmp isr_common
.endm

isr 0
isr 1
isr 2
isr 3
isr 4
isr 5
isr 6
isr 7
isr_error 8 #DF
# isr 9
isr_error 10 #TS
isr_error 11 #NP
isr_error 12 #SS
isr_error 13 #GP
isr_error 14 #PF
# More go here

isr 32

# 33 Used for timer initialization

.macro irq index
    .global _irq\index
    .type _irq\index, @function
    _irq\index:
    pushq $\index
    jmp irq_common
.endm

.altmacro
.set i, 34
.rept 221
    irq %i
    .set i, i+1
.endr

.global irq_pointers
# Array of irq function pointers
irq_pointers:

.macro irq_label index
    .quad _irq\index
.endm

.rept 34
    .quad 0x0
.endr
.set i, 34
.rept 221
    irq_label %i
    .set i, i+1
.endr


# Conditionally swap gs to ensure it is set to the kernel gs
# regardless of where the interupt was triggered
.macro swap_gs
    cli
    cmpq $8, 24(%rsp)
    je 1f
    swapgs
    1:
    #sti # Disabled to make kernel non preemptible
.endm


.global irq_pit
.type irq_pit, @function
irq_pit:
    swap_gs
    # Store registers
    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12
    pushq %r11
    pushq %r10
    pushq %r9
    pushq %r8
    pushq %rbp
    pushq %rdi
    pushq %rsi
    pushq %rdx
    pushq %rcx
    pushq %rbx
    pushq %rax

    .extern oneshot_triggered
    movb $1, oneshot_triggered

    .extern apic_send_eoi
    .type apic_send_eoi, @function
    call apic_send_eoi

    # Restore register values
    popq %rax
    popq %rbx
    popq %rcx
    popq %rdx
    popq %rsi
    popq %rdi
    popq %rbp
    popq %r8
    popq %r9
    popq %r10
    popq %r11
    popq %r12
    popq %r13
    popq %r14
    popq %r15

    swap_gs
    iretq

.global isr_common
.type isr_common @function
isr_common:
    swap_gs
    # Store registers
    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12
    pushq %r11
    pushq %r10
    pushq %r9
    pushq %r8
    pushq %rbp
    pushq %rdi
    pushq %rsi
    pushq %rdx
    pushq %rcx
    pushq %rbx
    pushq %rax

    cld # C assumes direction is clear

    # Handle the interrupt
    .extern interrupt_handler
    .type interrupt_handler, @function

    call interrupt_handler

    # Restore register values
    popq %rax
    popq %rbx
    popq %rcx
    popq %rdx
    popq %rsi
    popq %rdi
    popq %rbp
    popq %r8
    popq %r9
    popq %r10
    popq %r11
    popq %r12
    popq %r13
    popq %r14
    popq %r15

    swap_gs
    # Remove error and isr index
    add $16, %rsp
    # Exit handler
    iretq

.global isr_spurious
.type isr_spurious @function
isr_spurious:
    swap_gs

    # Store registers
    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12
    pushq %r11
    pushq %r10
    pushq %r9
    pushq %r8
    pushq %rbp
    pushq %rdi
    pushq %rsi
    pushq %rdx
    pushq %rcx
    pushq %rbx
    pushq %rax

    cld # C assumes direction is clear

    .extern apic_send_eoi
    .type apic_send_eoi, @function
    call apic_send_eoi

    # Restore register values
    popq %rax
    popq %rbx
    popq %rcx
    popq %rdx
    popq %rsi
    popq %rdi
    popq %rbp
    popq %r8
    popq %r9
    popq %r10
    popq %r11
    popq %r12
    popq %r13
    popq %r14
    popq %r15

    swap_gs
    iretq

.global irq_common
.type irq_common @function
irq_common:
    swap_gs

    # Store registers
    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12
    pushq %r11
    pushq %r10
    pushq %r9
    pushq %r8
    pushq %rbp
    pushq %rdi
    pushq %rsi
    pushq %rdx
    pushq %rcx
    pushq %rbx
    pushq %rax

    cld # C assumes direction is clear

    movq 0x78(%rsp), %rdi

    # If a registered handler is waiting on this interrupt, unblock it
    .extern interrupt_dispatch
    call interrupt_dispatch

    .extern apic_send_eoi
    .type apic_send_eoi, @function
    call apic_send_eoi

    # Restore register values
    popq %rax
    popq %rbx
    popq %rcx
    popq %rdx
    popq %rsi
    popq %rdi
    popq %rbp
    popq %r8
    popq %r9
    popq %r10
    popq %r11
    popq %r12
    popq %r13
    popq %r14
    popq %r15

    # Remove the irq number
    add $8, %rsp

    swap_gs
    iretq
