
#include <kernel/stack.h>
#include <kernel/arch/arch.h>
#include <arch/x86_64/gdt.h>
#include <stdint.h>

typedef struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t access;
    uint8_t granularity; // Lower half is last 4 bits of limit, upper half is flags
    uint8_t base_high;
//    uint32_t base_highest;
//    uint32_t reserved;
} __attribute__((packed)) gdt_entry;

// TSS descriptor, 2 entries large
typedef struct gdt_tss_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t access;
    uint8_t granularity; // Lower half is last 4 bits of limit, upper half is flags
    uint8_t base_high;
    uint32_t base_highest;
    uint32_t reserved;
} __attribute__((packed)) gdt_tss_entry;


typedef struct tss {
    uint32_t reserved;
    uint64_t rsp[3];
    uint64_t reserved_2;
    uint64_t ist[7]; // Interrupt stacks
    uint64_t resevred_3;
    uint16_t reserved_4;
    uint16_t iopb_offset;
} __attribute__((packed)) tss;

// Tbe gdt defined in gdt.S
extern gdt_entry gdt[];

tss boot_cpu_tss;

void init_tss() {

    gdt_tss_entry *tss_entry = (gdt_tss_entry*)&gdt[5];
    uintptr_t base = (uintptr_t)&boot_cpu_tss;
    tss_entry->base_low = base & 0xffff; // Low 16 bits
    tss_entry->base_mid = (base >> 16) & 0xff; // Next 8 bits
    tss_entry->base_high = (base >> 24) & 0xff; // Next 8 bits
    tss_entry->base_highest = (base >> 32); // Next 8 bits
    tss_entry->access = 0x89;
    tss_entry->limit_low = (sizeof(tss) -1) & 0xffff;
    tss_entry->granularity = ((sizeof(tss) - 1) >> 16) & 0xf;

    tss_entry->granularity |= 0b00010000;

    extern uint8_t stack;
    boot_cpu_tss.rsp[0] = ((uintptr_t)&stack) + BOOT_STACK_SIZE - 16;

    // Load the tss
    asm volatile ("mov $0x2b, %rax; ltr %ax");
}

void arch_set_interrupt_stack(uintptr_t stack_top) {
    boot_cpu_tss.rsp[0] = stack_top;
}
