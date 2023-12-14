// Warning: Included from assembly code

#ifndef ARCH_X86_64_DEFINES_H_
#define ARCH_X86_64_DEFINES_H_

#define PAGE_SIZE 4096 // 4 KB
#define MAX_CPUS_COUNT 32

#define NUMBER_OF_INTERRUPTS 256

#define KERNEL_VIRTUAL_ADDRESS  0xFFFFFFFF80000000ul
#define KERNEL_MEMORY_BASE      0xFFFF800000000000ul
#define KERNEL_MEMORY_LENGTH        0x800000000000ul
#define USER_MEMORY_LENGTH          0x800000000000ul

#define PER_THREAD_KERNEL_STACK_SIZE (PAGE_SIZE * 3)

#ifndef __ASSEMBLER__

struct arch_per_cpu_data {
    int local_apic_id;
};

#endif

#endif // ARCH_X86_64_DEFINES_H_
