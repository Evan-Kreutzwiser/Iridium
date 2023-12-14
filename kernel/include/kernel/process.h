
#ifndef KERNEL_PROCESS_H_
#define KERNEL_PROCESS_H_

#include "iridium/types.h"
#include "arch/registers.h"
#include "arch/defines.h"
#include "arch/address_space.h"
#include "kernel/memory/v_addr_region.h"
#include "kernel/linked_list.h"
#include "kernel/object.h"
#include "kernel/spinlock.h"

#include <stdbool.h>

struct process {

    object object;

    address_space address_space;
    struct v_addr_region *root_v_addr_region;

    linked_list threads;

    /// All open handles available to the process
    linked_list handle_table;
    /// New handles can use next_handle_id when free_handle_ids is empty
    atomic_long next_handle_id;
    /// The IDs are stored where the data pointer should go
    /// Utility for quickly determining the next ID to assign a handle.
    linked_list free_handle_ids;
    lock_t handle_table_lock;

    // Although the linked list struct has a lock already,
    // the methods automatically try to obtain it themselves
    // so its easier to add one we control outside the list.
    // TODO: Can I write things such that this is unnessecary?
};

/// @brief Thread kernel object
///
/// Parent process is `object.parent`
struct thread {
    object object;

    /// Loaded when entering syscalls
    uintptr_t kernel_stack_top;
    struct v_addr_region *kernel_stack;

    /// If a thread is currently running a syscall wait to kill it
    /// to avoid leaving the kernel in a bad state
    bool in_syscall;
    bool is_exiting;

    struct registers context;

    int thread_id;
};

extern struct process* idle_process;

void create_idle_process(void);

/// @brief Create an idle thread for a cpu to schedule when nothing else is available
struct thread *create_idle_thread(void);

ir_status_t process_create(struct process **process_out, struct v_addr_region **virtual_address_space_out);
ir_status_t thread_create(struct process *init_process, struct thread **out);

ir_status_t sys_thread_create(ir_handle_t parent_process, ir_handle_t *out);
ir_status_t sys_thread_start(ir_handle_t thread, uintptr_t entry, uintptr_t stack_top, uintptr_t arg0);

// Save an interrupt context to be reentered later
void thread_save_context(struct registers *context, struct thread *thread);

// SYSCALL_PROCESS_CREATE
ir_status_t sys_process_create(ir_handle_t *process, ir_handle_t *v_addr_region);

#endif // ! KERNEL_PROCESS_H_
