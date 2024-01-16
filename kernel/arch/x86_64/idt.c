
#include "arch/x86_64/idt.h"
#include "arch/x86_64/paging.h"
#include "arch/x86_64/acpi.h"
#include "arch/registers.h"
#include "kernel/arch/arch.h"
#include "kernel/interrupt.h"
#include "kernel/process.h"
#include "kernel/memory/physical_map.h"
#include "kernel/memory/vmem.h"
#include "kernel/main.h"
#include "kernel/string.h"
#include <stdbool.h>

#include <arch/debug.h>

struct stack_frame {
    struct stack_frame *rbp;
    uintptr_t rip;
};

/// The system's interrupt table
struct idt _idt;

/// @brief Print a stack trace in the form of an addr2line command.
///
/// The output can be copy/pasted into a terminal to get line numbers
/// for each address in the trace, sometimes one line ahead of where
/// the function was called.
void print_stack_trace(uintptr_t rbp, uintptr_t rip) {

    if (rip == 0 || rbp == 0) {
        debug_print("Stack trace impossible\n");
        return;
    }

    struct stack_frame *frame = (struct stack_frame*)rbp;

    debug_printf("Stack trace:\naddr2line -e kernel/kernel.sys %#.16p", rip);
    while ((unsigned long)frame > 0xFFFF800000000000ul)
    {
        debug_printf(" %#.16p", frame->rip);
        frame = frame->rbp;
    }
    debug_print_char('\n');
}

void dump_context(registers *context) {
    debug_printf("rip=%#.16p rsp=%#.16p rbp=%#.16p\n\n", context->rip, context->rsp, context->rbp);
    debug_printf("rax=%#.16p rbx=%#.16p rcx=%#.16p rdx=%#.16p\n", context->rax, context->rbx, context->rcx, context->rdx);
    debug_printf("rdi=%#.16p rsi=%#.16p  r8=%#.16p  r9=%#.16p\n", context->rdi, context->rsi, context->r8, context->r9);
    debug_printf("r10=%#.16p r11=%#.16p r12=%#.16p r13=%#.16p\n", context->r10, context->r11, context->r12, context->r13);
    debug_printf("r14=%#.16p r15=%#.16p rflags=%#.16p\n", context->r14, context->r15, context->rflags);
    debug_printf("CS=%#p SS=%#p\n", context->cs, context->ss);
}

void idt_set_entry(uint8_t index, uint16_t segment,
    uintptr_t entry_offset, idt_gate_type gate_type, idt_dpl dpl) {

    idt_entry *entry = &_idt.entries[index];

    entry->base_low = entry_offset & 0xffff; // Low 16 bits
    entry->base_mid = (entry_offset >> 16) & 0xffff; // Middle 16 bits
    entry->base_high = (entry_offset >> 32) & 0xffffffff; // High 32 bits

    entry->type = gate_type | (dpl << 5) | INTERRUPT_PRESENT;
    entry->segment = segment;
    entry->ist_index = 0;
}

/// @brief Generic exception handler
void exception(registers *context, char *name) {
    debug_printf("\n----------------\nException %#x!\n", context->interrupt_number);
    debug_printf("%s, error code %#x\n", name, context->error_code);
    debug_print("----------------\n");

    dump_context(context);
    print_stack_trace(context->rbp, context->rip);

    char buffer[150];
    sprintf(buffer, "CPU exception encountered\n\n"
            "%s at %#p, error code is %#x, if applicable.\n",
            name, context->rip, context->error_code
        );

    panic(context, 0xe, buffer);
}

void double_fault(registers *context) {
    debug_print("\n----------------\nDouble Fault!\n");
    debug_printf("An unrecoverable error occured at %#p.\n", context->rip);
    debug_print("Seeing this likely means a different error was not correctly handled.\n");
    debug_print("----------------\n");

    dump_context(context);
    print_stack_trace(context->rbp, context->rip);

    char buffer[200];
    sprintf(buffer, "Double Fault\n\n"
            "Encountered a critical error at %#p.\n\n"
            "Seeing this message means that another error failed to be handled correctly\n",
            context->rip
        );

    panic(context, 8, buffer);
}

void general_protection_fault(registers *context) {
    debug_print("\n----------------\nGeneral Protection Fault!\n");
    debug_printf("Encountered a segmentation-related error at %#p.\n", context->rip);

    char * cause = "Potential causes include referencing the null segment, writing to reserved control register bits,\naccessing a non-cannonical address, or other segment errors.\n";
    if (context->cs == 0x23 ) {
        cause = "The problem occured in user mode, so it may be the result of a program executing a privileged instruction or accessing a non-cannonical address.\n";
    }
    debug_print(cause);
    debug_printf("The segment selector, if applicable, is %#x.\n", context->error_code);
    debug_print("----------------\n");

    dump_context(context);
    print_stack_trace(context->rbp, context->rip);

    char buffer[400];
    sprintf(buffer, "General Protection Fault\n\n"
            "Encountered a segmentation-related error at %#p.\n\n"
            "%s\n"
            "The segment selector, if applicable, is %#x.\n",
            context->rip, cause, context->error_code
        );

    panic(context, 0xd, buffer);
}

static bool is_in_page_fault = false;

void page_fault(registers *context) {
    bool present = context->error_code & 0x1;
    bool write = context->error_code & (0x1 << 1);
    bool user = context->error_code & (0x1 << 2);
    bool reserved_bits = context->error_code & (0x1 << 3);
    bool instruction = context->error_code & (0x1 << 4);

    char *access_string = "read from";
    if (write) { access_string = "write to"; }
    if (instruction) { access_string = "run code at"; }

    char *page = "an unmapped page";
    if (present && instruction) { page = "a no-execute page"; }
    else if (reserved_bits) { page = "a page with reserved bits set"; }
    else if (present) { page = "a page with the wrong flags"; }

    char *ring = "Kernel";
    if (user) { ring = "User"; }

    uint64_t accessed_address;
    asm volatile ("mov %%cr2, %%rax; mov %%rax, %0;" : "=m" (accessed_address) :: "rax");

    debug_print("\n----------------\nPage Fault!\n");
    debug_printf("A paging related error was encountered at %#p, with error code %#x.\n", context->rip, (uint64_t)context->error_code);
    debug_printf("%s-space tried to %s %#p in %s.\n", ring, access_string, accessed_address, page);
    int thread_id = -1;
    if (this_cpu->current_thread != NULL) {
        thread_id = this_cpu->current_thread->thread_id;
        debug_printf("Occurred in thread %d\n", thread_id);
    }
    debug_print("----------------\n");

    dump_context(context);
    print_stack_trace(context->rbp, context->rip);


    // If this causes an exception don't try again
    if (!is_in_page_fault) {
        is_in_page_fault = true;
        uint64_t cr3;
        asm volatile ("mov %%cr3, %%rax; mov %%rax, %0;" : "=m" (cr3) :: "rax");
        paging_print_tables((page_table_entry*)p_addr_to_physical_map(cr3), accessed_address);
    }

    char buffer[300];
    sprintf(buffer, "Page fault\n\n"
            "A paging related error was encountered at %#p in thread %d, with error code %#x.\n"
            "%s-space tried to %s %#p in %s.\n",
            context->rip, thread_id, (uint64_t)context->error_code,
            ring, access_string, accessed_address, page
        );


    panic(context, 0xe, buffer);
}

void interrupt_handler(registers context) {

    if (context.interrupt_number >= 32)
        apic_send_eoi();

    switch (context.interrupt_number) {
        case 0x0:
            exception(&context, "Division By Zero");
            break;
        case 0x1:
            exception(&context, "Debug");
            break;
        case 0x2:
            exception(&context, "Non Maskable Interrupt");
            break;
        case 0x3:
            exception(&context, "Breakpoint");
            break;
        case 0x4:
            exception(&context, "Overflow");
            break;
        case 0x5:
            exception(&context, "Bound Range Exceeded");
            break;
        case 0x6:
            exception(&context, "Invalid Opcode");
            break;
        case 0x7:
            exception(&context, "Device Not Available");
            break;
        case 0x8:
            double_fault(&context);
            break;
        case 0xa:
            exception(&context, "Invalid TSS");
            break;
        case 0xb:
            exception(&context, "Segment Not Present");
            break;
        case 0xc:
            exception(&context, "Stack Segment Fault");
            break;
        case 0xd:
            general_protection_fault(&context);
            break;
        case 0xe:
            page_fault(&context);
            break;
        case 32:
            timer_fired(&context);
            break;
        default:
            exception(&context, "Unknown exeption!!");
            break;
    }

    // ???
    //return context;
}

extern void _isr0();
extern void _isr1();
extern void _isr2();
extern void _isr3();
extern void _isr4();
extern void _isr5();
extern void _isr6();
extern void _isr7();
extern void _isr8();
extern void _isr10();
extern void _isr11();
extern void _isr12();
extern void _isr13();
extern void _isr14();

extern void _isr32();

extern void _irq34();
extern void _irq35();
extern void _irq36();
extern void _irq37();
extern void _irq38();
extern void _irq39();
extern void _irq40();
extern void _irq41();
extern void _irq42();
extern void _irq43();
extern void _irq44();
extern void _irq45();

extern void isr_spurious();

void idt_init() {

    idt_set_entry(0, 0x8, (uintptr_t)&_isr0, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(1, 0x8, (uintptr_t)&_isr1, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(2, 0x8, (uintptr_t)&_isr2, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(3, 0x8, (uintptr_t)&_isr3, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(4, 0x8, (uintptr_t)&_isr4, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(5, 0x8, (uintptr_t)&_isr5, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(6, 0x8, (uintptr_t)&_isr6, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(7, 0x8, (uintptr_t)&_isr7, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(8, 0x8, (uintptr_t)&_isr8, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(0xa, 0x8, (uintptr_t)&_isr10, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(0xb, 0x8, (uintptr_t)&_isr11, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(0xc, 0x8, (uintptr_t)&_isr12, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(0xd, 0x8, (uintptr_t)&_isr13, IDT_GATE_INTERRUPT, 0);
    idt_set_entry(0xe, 0x8, (uintptr_t)&_isr14, IDT_GATE_INTERRUPT, 0);

    idt_set_entry(32, 0x8, (uintptr_t)&_isr32, IDT_GATE_INTERRUPT, 0);

    extern uintptr_t irq_pointers[256];
    for (int i = 34; i < 255; i++) {
        idt_set_entry(i, 0x8, irq_pointers[i], IDT_GATE_INTERRUPT, 0);
    }


    // Spurious interrupt vector
    idt_set_entry(0xff, 0x8, (uintptr_t)&isr_spurious, IDT_GATE_INTERRUPT, 0);

    idt_pointer idt_pointer = {
        .limit = sizeof(_idt) - 1,
        .base = &(_idt.entries[0])
    };

    asm volatile ("lidt (%0)" : : "r" (&idt_pointer));

    // Don't let users try to override the spurios interrupt vector
    interrupt_reserve(0xff);
}

/// Add an interrupt handler to the platform's interrupt table
void arch_interrupt_set(int vector, void *function) {
    // Interrupt set to only be triggered by hardware interrupts
    idt_set_entry(vector, 0x8, (uintptr_t)function, IDT_GATE_INTERRUPT, 0);
}

/// Remove an interrupt from the interrupt table
void arch_interrupt_remove(int vector) {
    memset(&_idt.entries[vector], 0, sizeof(struct idt_entry));
}
