/// @file kernel/scheduler.c
/// @brief Rudimentary round-robin scheduler

#include "kernel/scheduler.h"
#include "kernel/process.h"
#include "kernel/object.h"
#include "kernel/linked_list.h"
#include "kernel/arch/arch.h"
#include "kernel/arch/mmu.h"
#include "kernel/string.h"
#include "arch/registers.h"
#include "iridium/types.h"
#include "stdbool.h"

#include "arch/debug.h"

/// @brief Contains all threads waiting to run
/// Running threads are removed and at the end of their timeslice appended to the end
linked_list run_queue;

/// Every thread waiting on an object for signal changes.
/// Contains signal listeners rather than the thread itself.
/// TODO: sort in order of deadline
linked_list waiting_for_signals;

/// SYSCALL_YIELD
ir_status_t sys_yield() {

    // Prevent a timer interrupt from messing up context saving
    // Interrupts will be reenabled upon entering another process
    //arch_enter_critical();

    struct thread *thread = this_cpu->current_thread;

    // When the task resumes, return from this function
    arch_save_context(&thread->context);

    uintptr_t rip = (uintptr_t)arch_leave_function;
    arch_set_instruction_pointer(&thread->context, rip);

    // If for any reason the parent process needs to terminate, we should allow it as we no longer need its address space
    thread->in_syscall = false;
    //arch_exit_critical();
    switch_task(true);

    return IR_OK;
}

void switch_task(bool reschedule) {

    debug_print("Switching tasks\n");

    //arch_enter_critical();

    // Before switching tasks, see if there are any threads listening for signals whose deadlines have passed
    struct signal_listener *listener;
    uint64_t current_time = -1; // TODO: Deadline not supported yet, due to a lack of time tracking.
    while (linked_list_get(&waiting_for_signals, 0, (void*)&listener) == IR_OK && listener->deadline < current_time) {
        // TODO: Lock for multiprocessing environment
        linked_list_remove(&waiting_for_signals, 0, NULL);
        scheduler_unblock_listener(listener);
    }

    struct thread *thread = this_cpu->current_thread;
    struct thread *next;
    ir_status_t status = linked_list_remove(&run_queue, 0, (void**)&next);

    if (!thread) {
        debug_print("No thread\n");
    }

    // Retrieve and run the next thread
    if (status == IR_OK) {
        struct process *process = (struct process*)next->object.parent;
        arch_mmu_set_address_space(&process->address_space);
        arch_set_interrupt_stack(next->kernel_stack_top);
        this_cpu->current_thread = next;
        if (reschedule && thread != this_cpu->idle_thread) {
            linked_list_add(&run_queue, thread);
        }
        else if (reschedule){
            debug_print("Leaving idle thread\n");
        }

        //arch_exit_critical();
        arch_enter_context(&next->context);
    }
    else if (!reschedule || thread == this_cpu->idle_thread) {
        debug_print("No other threads, entering idle\n");
        linked_list_add(&run_queue, thread);
        this_cpu->current_thread = this_cpu->idle_thread;
        arch_set_interrupt_stack(this_cpu->idle_thread->kernel_stack_top);
        arch_mmu_enter_kernel_address_space();

        //arch_exit_critical();

        arch_enter_context(&this_cpu->idle_thread->context);
    }

    debug_print("No other threads, resuming current\n");

}

void schedule_thread(struct thread *thread) {
    linked_list_add(&run_queue, thread);
}

/// @brief Block a thread until a signal is set
/// @param listener The listener describing the signals that unblock the thread
void scheduler_block_listener_and_switch(struct signal_listener *listener) {

    // This most likely fails, because active threads are removed from the queue while running
    linked_list_find_and_remove(&run_queue, listener->thread, NULL, NULL);

    linked_list_add(&waiting_for_signals, listener->thread);
    switch_task(false);
}

/// @brief Unblock a thread that is listening to object signals
/// @param listener The thread's signal listener
void scheduler_unblock_listener(struct signal_listener *listener) {

    linked_list_find_and_remove(&waiting_for_signals, listener->thread, NULL, NULL);

    spinlock_aquire(listener->target->lock);
    linked_list_find_and_remove(&listener->target->signal_listeners, listener, NULL, NULL);
    spinlock_release(listener->target->lock);

    linked_list_add(&run_queue, listener->thread);
}
