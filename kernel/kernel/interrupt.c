
#include "kernel/interrupt.h"

#include "kernel/arch/arch.h"
#include "kernel/scheduler.h"
#include "kernel/handle.h"
#include "kernel/heap.h"
#include "iridium/types.h"
#include "iridium/errors.h"
#include "arch/registers.h"
#include "arch/defines.h"
#include "arch/debug.h"

struct interrupt *interrupts[NUMBER_OF_INTERRUPTS];

void interrupt_dispatch(int number) {
    struct interrupt *interrupt = interrupts[number];

    if (!interrupt) {
        debug_printf("WARNING: Interrupt %d fired without handler registered\n", number);
        return;
    }

    if (!interrupt->armed || !interrupt->thread) {
        debug_printf("WARNING: No thread listening for interrupt %d\n", number);
        return;
    }

    struct thread* thread = interrupt->thread;
    interrupt->thread = NULL;
    interrupt->armed = false;

    // TODO: Place at the begining of the queue so the interrupt is handled faster
    schedule_thread(thread);
}

ir_status_t interrupt_create(int vector, struct interrupt **out) {

    if (interrupts[vector]) {
        debug_printf("Failed to register interrupt %d, already points to %#p\n", vector, interrupts[vector]);
        return IR_ERROR_ALREADY_EXISTS;
    }

    struct interrupt *obj = calloc(1, sizeof(struct interrupt));
    interrupts[vector] = obj;

    obj->object.type = OBJECT_TYPE_INTERRUPT;

    // Dispatch already exists for most vectors

    *out = obj;
    return IR_OK;
}

/// @brief Allow the kernel to reserve interrupt vectors for hardware it controls
/// @param vector The interrupt to reserve
ir_status_t interrupt_reserve(int vector) {

    if (interrupts[vector])
        // Should not be encountered assuming the kernel reserves the interrupt before begining the init process
        return IR_ERROR_ALREADY_EXISTS;

    debug_printf("Reserving interrupt vector %d\n", vector);

    interrupts[vector] = (void*)0xDEADBEAF;
    return IR_OK;
}

/// @brief Facilitates garbage collection of interrupt objects
/// @param interrupt An interrupt with no references or listeners
void interrupt_cleanup(struct interrupt *interrupt) {
    // arch_interrupt_remove(interrupt->vector);
    int vector = interrupt->vector;
    arch_interrupt_remove(interrupt->vector);
    interrupts[vector] = NULL;
    // There shouldn't be any left over signal listeners if this is being garbage collected
    free(interrupt);
}

ir_status_t interrupt_wait(struct interrupt *interrupt) {

    interrupt->thread = this_cpu->current_thread;
    interrupt->armed = true;

    // Save the thread context
    arch_save_context(&this_cpu->current_thread->context);
    arch_set_instruction_pointer(&this_cpu->current_thread->context, (uintptr_t)arch_leave_function);

    // Stop executing the thread until the interrupt is triggered
    switch_task(false);

    return IR_OK;
}

ir_status_t sys_interrupt_create(int vector, ir_handle_t *out) {
    struct process* process = (struct process*)this_cpu->current_thread->object.parent;

    struct interrupt* interrupt;
    ir_status_t status = interrupt_create(vector, &interrupt);
    if (status != IR_OK)
        return status;

    struct handle* handle;
    handle_create(process, (struct object*)interrupt, IR_RIGHT_ALL, &handle);
    linked_list_add_sorted(&process->handle_table, handle_by_id, handle);

    *out = handle->handle_id;
    return IR_OK;
}

ir_status_t sys_interrupt_wait(ir_handle_t interrupt_handle) {
    struct process* process = (struct process*)this_cpu->current_thread->object.parent;

    spinlock_aquire(process->handle_table_lock);
    struct handle* handle;
    ir_status_t status = linked_list_find(&process->handle_table, (void*)interrupt_handle, handle_by_id, NULL, (void**)&handle);
    struct interrupt* interrupt = (struct interrupt*)handle->object;
    spinlock_release(process->handle_table_lock);
    if (status != IR_OK) {
        return IR_ERROR_BAD_HANDLE;
    }
    if (interrupt->object.type != OBJECT_TYPE_INTERRUPT) {
        return IR_ERROR_WRONG_TYPE;
    }

    interrupt->armed = true;

    return interrupt_wait(interrupt);
}

/// @brief Arm an interrupt to continue receiving interrupts, but does not block the current thread.
/// @param interrupt_handle Handle ID of the interrupt to re-arm
/// @return `IR_OK` on success, or an error code
ir_status_t sys_interrupt_arm(ir_handle_t interrupt_handle) {
    struct process* process = (struct process*)this_cpu->current_thread->object.parent;

    spinlock_aquire(process->handle_table_lock);
    struct handle* handle;
    ir_status_t status = linked_list_find(&process->handle_table, (void*)interrupt_handle, handle_by_id, NULL, (void**)&handle);
    struct interrupt* interrupt = (struct interrupt*)handle->object;
    spinlock_release(process->handle_table_lock);
    if (status) {
        return IR_ERROR_BAD_HANDLE;
    }
    if (interrupt->object.type != OBJECT_TYPE_INTERRUPT) {
        return IR_ERROR_WRONG_TYPE;
    }

    interrupt->armed = true;
    return IR_OK;
}
