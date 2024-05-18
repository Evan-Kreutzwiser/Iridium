// Entry point(s) and bootloader headers

#include <multiboot/multiboot2.h>
#include <kernel/stack.h>

#include <arch/defines.h>
#include <arch/x86_64/msr.h>

.section .multiboot
.type multiboot_header, @object
.size multiboot_header, multiboot2_header_end - multiboot2_header
multiboot2_header:
    // Base header
    .align  MULTIBOOT_HEADER_ALIGN
    .long   MULTIBOOT2_HEADER_MAGIC
    .long   0 // x86 architecture

    .equ    MULTIBOOT_HEADER_LENGTH, multiboot2_header_end - multiboot2_header
    .long   MULTIBOOT_HEADER_LENGTH

    .equ    MULTIBOOT_HEADER_CHECKSUM, -(MULTIBOOT2_HEADER_MAGIC + MULTIBOOT_HEADER_LENGTH)
    .long   MULTIBOOT_HEADER_CHECKSUM // Checksum - These 4 fields must equal 0 mod 2^32

    // Tags
    // Bit 0 of flags indicates whether bootloader support for the tag is optional

    // Information request tag
    .align 8
    .word MULTIBOOT_HEADER_TAG_INFORMATION_REQUEST
    .word 0 // flags
    .long 28
    .long MULTIBOOT_TAG_TYPE_FRAMEBUFFER
    .long MULTIBOOT_TAG_TYPE_ACPI_OLD
    .long MULTIBOOT_TAG_TYPE_ACPI_NEW
    .long MULTIBOOT_TAG_TYPE_MMAP
    .long MULTIBOOT_TAG_TYPE_MODULE

    // Framebuffer tag
    .align 8
    .word MULTIBOOT_HEADER_TAG_FRAMEBUFFER
    .word 0
    .long 20 // Tag is 20 bytes large
    .long 1920 // No prefered resolution (width)
    .long 1080 //                        (height)
    .long 32 // Prefer 32 bits per pixel

    // Page-align modules
    .align 8
    .word MULTIBOOT_HEADER_TAG_MODULE_ALIGN
    .word 0
    .long 8

    // Terminating tag
    .align 8
    .word MULTIBOOT_HEADER_TAG_END
    .word 0
    .long 8

multiboot2_header_end:

// Bootstrap page tables for entering 64 bit mode
.section .boot
.type bootstrap_page_tables, @object
.size bootstrap_page_tables, bootstrap_page_tables_end - bootstrap_page_tables
.align 0x1000
bootstrap_page_tables:
// These should only be used with bootloaders that don't put the cpu in 64 bit mode,
// And should be discarded as soon as memory systems are online and a new one can be allocated

page_map_level_4:
    // 4 KB of zero'd memory for each level of the table
    // Plus some entries that map everything to both the lower and higher halves
	.quad page_directory_pointer + 3
    .fill   510, 8, 0x0
	.quad page_directory_pointer + 3 // Higher half memory
page_directory_pointer:
	.quad page_directory + 3 // Identity mapped
    .fill   509, 8, 0x0
    .quad page_directory + 3 // Higher half
    .fill   1, 8, 0x0
page_directory:
	.quad 0x0000000000000083 // 0MB - 2MB
	.quad 0x0000000000200083 // 2MB - 4MB
	.quad 0x0000000000400083 // 4MB - 6MB
    .fill   509, 8, 0x0
bootstrap_page_tables_end:

// This is the entry point when booted using multiboot
// The multiboot spec provides an environment with flat,
// 32 bit segments, but no stack
.section .boot
.global multiboot_start
.type multiboot_start, @function
.code32
multiboot_start:
    cli

    // Make no assumptions about the environment
    cld // The abi requires the direction flag is clear

    // Multiboot spec requires there to be magic value in %eax
    cmp     $MULTIBOOT2_BOOTLOADER_MAGIC,    %eax
    jne     bad_multiboot_magic
    jmp     multiboot_trampoline_32


bad_multiboot_magic:
    ud2
    jmp     bad_multiboot_magic

// Get the cpu reeady for and into 64 bit mode
multiboot_trampoline_32:

    lgdt    gdt_pointer_32 - KERNEL_VIRTUAL_ADDRESS // Load the 64 bit segment descriptors

    // Enable PAE (Physical Addresss Extension) and PSE (Page Size Extension)
    movl    %cr4, %eax
    orl     $0b10110000, %eax
    movl    %eax, %cr4

    // Load temporary page tables that will support the transition to higher-half
    movl    $bootstrap_page_tables, %eax
    movl    %eax, %cr3

    // Enable long mode. Other features in this register will be enabled in the bootloader-generic bring-up code
    // in the Extended Feature Enable Register (EFER)
    movl    $MSR_EFER, %ecx
    rdmsr
    orl     $(MSR_EFER_LONG_MODE), %eax
    wrmsr

    // Turn on paging
	movl    %cr0, %eax
	orl     $0x80000000, %eax
	movl    %eax, %cr0

    // Jump to 64 bit mode
    ljmp    $0x8, $(multiboot_trampoline_64_low)

.code64
multiboot_trampoline_64_low:
    // Now that we're in long mode with paging enabled the higher half addresses are accessible
    mov     $multiboot_trampoline_64, %rax
    jmp     *%rax

// Higher half kernel code
.section .text
multiboot_trampoline_64:

    // Load 64 bit data segments
    // (We're already using a 64 bit code segment from the jump)
    lgdt    gdt_pointer_64
    mov     $0x10, %rax
    mov     %ax, %ds
    mov     %ax, %es
    mov     %ax, %ss
    // IRETQ sets the descriptors to null, which on intel CPUs (but not AMD) also zeros the base address.
    // Setting the segments to the null segment from the begining prevents this behavior.
    mov     $0x0, %rax
    mov     %ax, %fs
    mov     %ax, %gs
    // Loading the TSS segment will be dealt with once they can be properly allocated and set up

    // Finally setup a bootstrap stack to complete the C abi
    mov     $(boot_stack + BOOT_STACK_SIZE), %rsp
    mov     %rsp, %rbp

    call    debug_init;

    // Enable syscall instructions and no-execute pages
    movl    $MSR_EFER, %ecx
    rdmsr
    orl     $(MSR_EFER_SYSCALL | MSR_EFER_EXECUTE_DISABLE), %eax
    wrmsr

    // Set up system entry point
    movl    $MSR_STAR, %ecx
    xor     %eax, %eax // No 32 bit entry point
    movl    $0x00130008, %edx
    // On Syscall, CS  will be set to 32:47,
    // and SS will be set to 32:47 + 8
    // On sysret,  SS will be set to 63:48 + 8,
    // and CS will be set to 64:47 + 16
    wrmsr

    // GCC won't split the address up for us at compile time
    .extern syscall_entry
    movq    $syscall_entry, %rax
    movq    %rax, %rdx
    shr     $32, %rdx
    movl    $MSR_LSTAR, %ecx // Set 64 bit entry point
    wrmsr

    // Disable interrupts momentarily when entering a syscall,
    // so we don't get interrupted while switching gs and end up in a bad state
    movl    $MSR_SFMASK, %ecx
    movl    $(1 << 9), %eax
    xor     %edx, %edx
    wrmsr

    // Fully setup the legacy PIC, even if just emulated, to ensure the interrupt masking is applying properly
    #define PIC_MASTER_CMD $0x20
    #define PIC_MASTER_DATA $0x21
    #define PIC_SLAVE_CMD $0xA0
    #define PIC_SLAVE_DATA $0xA1
    mov $0x11, %al
    out  %al, PIC_MASTER_CMD
    out  %al, PIC_SLAVE_CMD

    // Map interrupts to the range 35-42, which we will ignore
    // in the event of spurious interrupts
    // (33 and 34 are being used already. TODO: Dynamically assign the keyboard interrupt)
    mov $35, %al
    out  %al, PIC_MASTER_DATA
    out  %al, PIC_SLAVE_DATA

    mov $0x4, %al
    out %al, PIC_MASTER_DATA
    mov $0x2, %al
    out %al, PIC_SLAVE_DATA

    mov $0x01, %al
    out %al, PIC_MASTER_DATA
    out %al, PIC_SLAVE_DATA

    // Mask all interrupts
    mov $0xFF, %al
    out %al, PIC_MASTER_DATA
    out %al, PIC_SLAVE_DATA


    // Set a null stack trace for debugging
    xor     %rbp, %rbp

    // Pass multiboot struct to C
    mov     %rbx, %rdi
    call    arch_main

    // Should not return, so if is does then stop the computer

    halt:
    cli
    hlt
    jmp halt

.global arch_enter_context
.type arch_enter_context, @function
arch_enter_context:
    cli // Interrupts will be reenabled upon iret from the thread's rflags

    movl 0xbc(%rdi), %edx
    movl 0xb8(%rdi), %eax
    movl $MSR_KERNEL_GS_BASE, %ecx
    wrmsr
    // Don't switch the GS base if we're staying in the kernel
    cmpq $0x8, 0x90(%rdi)
    je 1f
    swapgs
    1:

    movq 0x8(%rdi), %rbx
    movq 0x10(%rdi), %rcx
    movq 0x18(%rdi), %rdx
    movq 0x20(%rdi), %rsi
    // rdi set last
    movq 0x30(%rdi), %rbp

    movq 0x38(%rdi), %r8
    movq 0x40(%rdi), %r9
    movq 0x48(%rdi), %r10
    movq 0x50(%rdi), %r11
    movq 0x58(%rdi), %r12
    movq 0x60(%rdi), %r13
    movq 0x68(%rdi), %r14
    movq 0x70(%rdi), %r15

    // Data segments should match ss
    movq 0xa8(%rdi), %rax
    movq %rax, %ds
    movq %rax, %es

    // Create stack frame for returning from interrupt
    pushq %rax // ss
    pushq 0xa0(%rdi) // rsp
    pushq 0x98(%rdi) // rflags
    pushq 0x90(%rdi) // cs
    pushq 0x88(%rdi) // rip

    // rax and rdi have to be restored last since they're used to restore other registers
    movq (%rdi), %rax
    movq 0x20(%rdi), %rdi

    iretq

.global arch_save_context
.type arch_save_context, @function
/// struct registers *context in rdi
arch_save_context:

    movq %rax, (%rdi)
    movq %rbx, 0x8(%rdi)
    movq %rcx, 0x10(%rdi)
    movq %rdx, 0x18(%rdi)
    movq %rdi, 0x20(%rdi) // Contains pointer to where context is being saved
    movq %rsi, 0x28(%rdi)
    movq %rbp, 0x30(%rdi)

    movq %r8, 0x38(%rdi)
    movq %r9, 0x40(%rdi)
    movq %r10, 0x48(%rdi)
    movq %r11, 0x50(%rdi)
    movq %r12, 0x58(%rdi)
    movq %r13, 0x60(%rdi)
    movq %r14, 0x68(%rdi)
    movq %r15, 0x70(%rdi)

    // Use the return pointer to make the saved context
    // continue immediately after this function
    movq (%rsp), %rax
    movq %rax, 0x88(%rdi)

    movq %cs, %rax
    movq %rax, 0x90(%rdi)

    pushfq
    pop %rax
    movq %rax, 0x98(%rdi) // Rflags

    movq %rsp, 0xa0(%rdi)

    // All data segments are set to the same value as ss
    movq %ss, %rax
    movq %rax, 0xa8(%rdi)

    ret


.global arch_leave_function
.type arch_leave_function, @function
// Helper to clean up the stack properly after a task switch
// NOTE: Doesn't pop callee preserved registers (Not something I can easily fix).
// Hopefully not a problem since its only used when returning from a manual scheduler
// invokation, and a userspace context is restored immediately after.
arch_leave_function:
    leave
    mov $0, %rax // Return IR_OK
    ret
