#ifndef KERNEL_CPU_LOCALS_H_
#define KERNEL_CPU_LOCALS_H_

#include "arch/defines.h"

struct thread; // #include "kernel/process.h"

struct per_cpu_data {
    // Note: Be take care to update the offset in the x86_64 syscall entry point if current_thread moves or is changed
    struct thread *volatile current_thread;
    struct thread *idle_thread;
    int core_id;
    struct arch_per_cpu_data arch;
};

#ifdef __x86_64__
// Accessing this reads from the location set in GS on x86_64, which will be different for every core
static struct per_cpu_data __seg_gs * const this_cpu = 0;
#endif

// Stored in process.c
extern struct per_cpu_data processor_local_data[MAX_CPUS_COUNT];
extern int cpu_count;

#endif // KERNEL_CPU_LOCALS_H
