/// @file arch/registers.h
/// @brief Architecture-specific cpu context struct

#ifndef ARCH_REGISTERES_H_
#define ARCH_REGISTERES_H_

#include <stdint.h>

/// @brief Arch-specific struct containing cpu context saved during scheduling and interrupts
typedef struct registers {

    // Pushed by common interrupt stub
    uint64_t rax, rbx, rcx, rdx;
    uint64_t rsi, rdi, rbp;
    uint64_t r8, r9, r10, r11;
    uint64_t r12, r13, r14, r15;

    // Interupt info
    uint64_t interrupt_number, error_code;

    // Context pushed by CPU
    uint64_t rip, cs, rflags, rsp, ss;

    // Other thread context
    uint64_t fs_base, gs_base;
} registers;

#endif // ARCH_REGISTERES_H_
