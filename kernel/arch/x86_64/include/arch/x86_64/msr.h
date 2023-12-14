#ifndef ARCH_X86_64_MSR_H_
#define ARCH_X86_64_MSR_H_

// CPU features enabling register
#define MSR_EFER 0xC0000080
// EFER MSR feature bits
#define MSR_EFER_SYSCALL (1) // Enables the use of syscalls
#define MSR_EFER_LONG_MODE (1 << 8) // Switches the CPU to long mode (64 bit)
#define MSR_EFER_LONG_MODE_ACTIVE (1 << 10) // Indicates that the CPU is currently in long mode
#define MSR_EFER_EXECUTE_DISABLE (1 << 11) // Allows pages to be marked as non executable

// System call setup
#define MSR_STAR            0xC0000081 // Syscall/sysret base segments and 32 bit syscall entry point
#define MSR_LSTAR           0xC0000082 // 64 bit syscall entry point
#define MSR_CSTAR           0xC0000083 // Compatibility mode syscall entry point (64 bit kernel, 32 bit user)
#define MSR_SFMASK          0xC0000084 // Syscall flag mask

// Segment based
#define MSR_FS_BASE         0xC0000100
#define MSR_GS_BASE         0xC0000101
#define MSR_KERNEL_GS_BASE  0xC0000102

// ACPI
#define MSR_APIC_BASE       0x1B // Contains the physical address of the local apic mmio registers
                                 // Every cpu has its own local apic mapped to the same address
#define MSR_APIC_BASE_ENABLE 0x800

#ifndef __ASSEMBLER__

#include <stdint.h>

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t high;
    uint32_t low;
    asm volatile ("rdmsr" : "=d" (high), "=a" (low) : "c" (msr));
    return (uint64_t)high << 32 | low;
}

static inline void wrmsr(uint32_t msr, uint64_t data) {
    asm volatile ("wrmsr" : : "c" (msr), "d" (data >> 32), "a" (data));
}

#endif // ! __ASSEMBLER__

#endif // ! ARCH_X86_64_MSR_H_
