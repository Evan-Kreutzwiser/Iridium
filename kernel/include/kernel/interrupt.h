
#ifndef KERNEL_INTERRUPT_H_
#define KERNEL_INTERRUPT_H_

#include "kernel/object.h"
#include "iridium/types.h"
#include <stdbool.h>

// kernel/process.h
struct thread;

/// @brief Represents an interrupt registered with the operating system.
/// The interrupt firing activates a signal under the object, triggering
/// a callback in the user process
struct interrupt {
    object object;

    /// Threads listening for interrupts. Having a larger
    //linked_list threads;
    struct thread* thread;
    /// If there are no threads available to take the interrupt, a backlog builds here.
    /// Once this is full interrupts are lost.
    /// TODO: Queue is unused, interrupts are lost
    linked_list queue;
    /// Index into the platform's interrupt table
    int vector;
    bool armed;
};

ir_status_t interrupt_create(int vector, struct interrupt **out);
/// @brief Reserve interrupt vectors for the kernel, such that processes cant use them
ir_status_t interrupt_reserve(int vector);

void interrupt_dispatch(int number);

ir_status_t interrupt_wait(struct interrupt *interrupt);
ir_status_t interrupt_arm(struct interrupt *interrupt);

void interrupt_cleanup(struct interrupt *interrupt);

ir_status_t sys_interrupt_create(int vector, ir_handle_t *out);
ir_status_t sys_interrupt_wait(ir_handle_t interrupt_handle);
ir_status_t sys_interrupt_arm(ir_handle_t interrupt_handle);

#endif // KERNEL_INTERRUPT_H_
