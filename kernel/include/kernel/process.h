
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

enum termination_state {
    /// The process is running as per usual
    ACTIVE,
    /// The process is ending but has not completed termination
    TERMINATING,
    /// The process is killed, and an exit code is available
    TERMINATED
};

/// @brief Contains processes and sub-tasks, forming a hierarchy
struct task {
    /// Both sub-tasks and processes count as children
    object object;

    enum termination_state state;
    /// TODO: Scheduler influencing settings?
};

struct process {

    /// Threads are children of the object
    object object;

    address_space address_space;
    struct v_addr_region *root_v_addr_region;

    enum termination_state state;
    size_t exit_code;

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
    enum termination_state state;
    size_t exit_code;

    struct registers context;
    /// If the thread is sleeping, this is the time since boot
    /// in microseconds when it will wake up
    size_t sleeping_until;
    /// Set when the thread is listening for signals in another object.
    struct signal_listener* blocking_listener;

    /// If a thread is currently running a syscall wait to kill it
    /// to avoid leaving the kernel in a bad state
    bool in_syscall;
    int thread_id;
};

extern struct process* idle_process;

void create_idle_process(void);

/// @brief Create an idle thread for a cpu to schedule when nothing else is available
struct thread *create_idle_thread(void);

ir_status_t task_create(struct task *parent);
ir_status_t process_create(struct process **process_out, struct v_addr_region **virtual_address_space_out);
ir_status_t thread_create(struct process *init_process, struct thread **out);


ir_status_t thread_start(struct thread *thread, uintptr_t entry, uintptr_t stack_top, uint64_t arg0);

/// Begin termination of a process
/// Object must be locked before calling
void process_kill_locked(struct process *process, long exit_code);

/// Remove the memory backing a process and release its held handles
/// Note: Only call when all threads have been terminated
void process_finish_termination(struct process *process);
/// Use to transition an ending thread from `TERMINATING` to `TERMINATED`
void thread_finish_termination(struct thread *thread);

/// @brief Process garbage collection handler
void process_cleanup(struct process *process);
/// @brief Thread garbage collection handler
void thread_cleanup(struct thread *thread);

/// SYSCALL_PROCESS_CREATE
ir_status_t sys_process_create(ir_handle_t *process, ir_handle_t *v_addr_region);

/// SYSCALL_THREAD_CREATE
ir_status_t sys_thread_create(ir_handle_t parent_process, ir_handle_t *out);
/// SYSCALL_THREAD_START
ir_status_t sys_thread_start(ir_handle_t thread, uintptr_t entry, uintptr_t stack_top, uintptr_t arg0);

/// @brief SYSCALL_PROCESS_EXIT
/// NOTE: Other running threads in this process will finish their time
/// slice or previously started system calls before exiting
ir_status_t sys_process_exit(long exit_code);
/// SYSCALL_THREAD_EXIT
ir_status_t sys_thread_exit(long exit_code);

#endif // ! KERNEL_PROCESS_H_
