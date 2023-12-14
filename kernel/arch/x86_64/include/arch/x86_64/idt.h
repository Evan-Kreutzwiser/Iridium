
#ifndef ARCH_X86_64_IDT_H_
#define ARCH_X86_64_IDT_H_

#include <stdint.h>

#define INTERRUPT_PRESENT (0x1 << 7)

typedef enum idt_gate_type {
    IDT_GATE_INTERRUPT = 0xe, // For hardware interrupts
    IDT_GATE_TRAP = 0xf // For cpu exceptions
} idt_gate_type;

typedef enum idt_dpl {
  IDT_DPL_0 = 0,
  IDT_DPL_1 = 1,
  IDT_DPL_2 = 2,
  IDT_DPL_3 = 3,
} idt_dpl;

typedef struct idt_entry {
    uint16_t base_low;
    uint16_t segment;
    uint8_t ist_index; // Index in the interrupt stack table
    uint8_t type; // Gate type amd presence bit
    uint16_t base_mid;
    uint32_t base_high;
    uint32_t reserved2;
} __attribute__((packed)) idt_entry;

typedef struct idt_pointer {
    uint16_t limit;
    idt_entry *base;
} __attribute__((packed)) idt_pointer;

struct idt {
    idt_entry entries[256];
} __attribute__((packed));

void idt_set_entry(uint8_t index, uint16_t segment, 
    uintptr_t entry_offset, idt_gate_type gate_type, idt_dpl dpl);

void idt_init();

#endif // ARCH_X86_64_IDT_H_