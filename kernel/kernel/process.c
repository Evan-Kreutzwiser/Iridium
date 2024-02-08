/// @file kernel/process.c
/// Process and thread objects and management functions

#include "kernel/process.h"
#include "kernel/cpu_locals.h"
#include "kernel/channel.h"
#include "kernel/handle.h"
#include "kernel/scheduler.h"
#include "kernel/memory/v_addr_region.h"
#include "kernel/arch/arch.h"
#include "kernel/arch/mmu.h"
#include "kernel/string.h"
#include "kernel/heap.h"
#include "iridium/errors.h"
#include "arch/defines.h"
#include "arch/debug.h"

#define __need_null
#include <stddef.h>

/// Holds the processor specific data referenced using `this_cpu`
struct per_cpu_data processor_local_data[MAX_CPUS_COUNT];
/// Number of logical processors ("threads") in the computer
int cpu_count;

int next_thread_id = 1;

/// Parent of each CPU's idle thread
struct process* idle_process;

/// @brief This function is run when scheduling the idle task
void idle_task() {
    while (1) {
        arch_pause();
    }
}

/// @brief Create a process to hold each CPU's idle thread
void create_idle_process() {
    struct process* process = calloc(1, sizeof(struct process));
    process->address_space = *get_kernel_address_space();
    idle_process = process;
}

/// @brief Create an idle thread to run when nothing else is scheduled
struct thread* create_idle_thread() {
    // Since this is only called during startup we don't need to do error checking
    struct thread* idle_thread = calloc(1, sizeof(struct thread));

    linked_list_add(&idle_process->object.children, idle_thread);
    idle_thread->object.parent = (object*)idle_process;

    idle_thread->thread_id = 0;

    vm_object *kernel_stack_vm;
    v_addr_t stack_base;
    vm_object_create(PER_THREAD_KERNEL_STACK_SIZE, VM_READABLE | VM_WRITABLE, &kernel_stack_vm);
    v_addr_region_map_vm_object(kernel_region, V_ADDR_REGION_READABLE | V_ADDR_REGION_WRITABLE,
        kernel_stack_vm, &idle_thread->kernel_stack, 0, &stack_base);
    // 16 Byte align the top of the stack without entering the next page
    idle_thread->kernel_stack_top = stack_base + PER_THREAD_KERNEL_STACK_SIZE - 16;
    // The scheduler will not save context for this task,
    // and when the thread is run it will enter `idle_task()`

    arch_set_instruction_pointer(&idle_thread->context, (uintptr_t)idle_task);
    arch_set_stack_pointer(&idle_thread->context, stack_base + PER_THREAD_KERNEL_STACK_SIZE - 16);
    arch_initialize_thread_context(&idle_thread->context, true);

    return idle_thread;
}

/// @brief Create a blank process
/// @note The process begins with 2 handles: Itself, and its root v_addr_region.
/// @param process_out Output parameter set to the newly created process object
/// @param virtual_address_space_out Output parameter set to the process's root `v_addr_region`
/// @param channel_out Output parameter providing a channel for passing arguments and additional handles
/// @return `IR_OK` on success, or `IR_ERROR_NOMEMORY` under out-of-memory conditions
ir_status_t process_create(struct process **process_out, struct v_addr_region **virtual_address_space_out, struct channel **channel_out) {

    struct process *process = calloc(1, sizeof(struct process));
    process->object.type = OBJECT_TYPE_PROCESS;
    struct channel *channel, *channel_peer;

    ir_status_t status = arch_mmu_create_address_space(&process->address_space);
    if (status != IR_OK) {
        free(process);
        return status;
    }
    status = channel_create(&channel, &channel_peer);
    if (status != IR_OK) {
        free(process);
        return status;
    }

    status = v_addr_region_create_root(&process->address_space, 0, USER_MEMORY_LENGTH, &process->root_v_addr_region);
    if (status != IR_OK) {
        free(process);
        free(channel);
        free(channel_peer);
        return status;
    }

    // Each process's first 3 handles are for their own process and address space,
    // and a channel for passing arguments
    struct handle *process_handle;
    struct handle *v_addr_region_handle;
    struct handle *channel_handle;
    handle_create(process, (object*)process, IR_RIGHT_ALL, &process_handle);
    handle_create(process, (object*)process->root_v_addr_region, IR_RIGHT_ALL, &v_addr_region_handle);
    handle_create(process, (object*)channel_peer, IR_RIGHT_ALL, &channel_handle);
    linked_list_add(&process->handle_table, process_handle);
    linked_list_add(&process->handle_table, v_addr_region_handle);
    linked_list_add(&process->handle_table, channel_handle);

    *process_out = process;
    *virtual_address_space_out = process->root_v_addr_region;
    *channel_out = channel;
    return IR_OK;
}

/// @brief Create a new thread
/// @param parent_process The process to spawn a new thread under
/// @param out Output parameter set to the new thread
/// @return `IR_OK` on success, or an error code
ir_status_t thread_create(struct process* parent_process, struct thread **out) {

    struct thread *thread = calloc(1, sizeof(struct thread));
    if (!thread) { return IR_ERROR_NO_MEMORY; }

    spinlock_aquire(parent_process->object.lock);

    if (linked_list_add(&parent_process->object.children, thread) != IR_OK) {
        spinlock_release(parent_process->object.lock);
        free(thread);
        return IR_ERROR_NO_MEMORY;
    }

    thread->object.type = OBJECT_TYPE_THREAD;
    thread->object.parent = (object*)parent_process;
    arch_initialize_thread_context(&thread->context, false);
    thread->thread_id = next_thread_id;
    next_thread_id++;
    vm_object *kernel_stack_vm;
    v_addr_t stack_base;
    if (vm_object_create(PER_THREAD_KERNEL_STACK_SIZE, VM_READABLE | VM_WRITABLE, &kernel_stack_vm) == IR_OK) {
        if (v_addr_region_map_vm_object(kernel_region, V_ADDR_REGION_READABLE | V_ADDR_REGION_WRITABLE,
                kernel_stack_vm, &thread->kernel_stack, 0, &stack_base) == IR_OK) {

            parent_process->object.references++;
            spinlock_release(parent_process->object.lock);
            // 16 Byte align the top of the stack without entering the next page
            thread->kernel_stack_top = stack_base + PER_THREAD_KERNEL_STACK_SIZE - 16;

            *out = thread;
            return IR_OK;
        } else {
            vm_object_cleanup(kernel_stack_vm);
        }
    }

    linked_list_find_and_remove(&parent_process->object.children, thread, NULL, NULL);
    spinlock_release(parent_process->object.lock);
    free(thread);

    return IR_ERROR_NO_MEMORY;
}

/// @brief Begin execution of a thread
/// @param thread
/// @param entry The function where the thread begins execution
/// @param stack_top
/// @param arg0 A value passed as an argument to the entry function
/// @return
ir_status_t thread_start(struct thread *thread, uintptr_t entry, uintptr_t stack_top, uint64_t arg0) {

    arch_set_instruction_pointer(&thread->context, entry);
    arch_set_stack_pointer(&thread->context, stack_top);
    arch_set_arg_0(&thread->context, arg0);

    // Executing contexts keep themselves alive
    thread->object.references++;

    schedule_thread(thread);

    return IR_OK;
}

/// @brief SYSCALL_PROCESS_CREATE
/// @param process Output parameter set to the new process's handle
/// @param v_addr_region Output parameter giving access to the process's address space
/// @param channel Output parameter providing a channel for passing arguments to the process
/// @return On success, returns `IR_OK` and process and v_addr_region are valid handle ids.
ir_status_t sys_process_create(ir_handle_t *process, ir_handle_t *v_addr_region, ir_handle_t *channel) {
    if (!arch_validate_user_pointer(process) || !arch_validate_user_pointer(v_addr_region)) {
        return IR_ERROR_INVALID_ARGUMENTS;
    }

    struct process *current_process = (struct process*)this_cpu->current_thread->object.parent;

    struct process *new_process;
    struct v_addr_region *root_region;
    struct channel *startup_channel;
    process_create(&new_process, &root_region, &startup_channel);

    struct handle *process_handle;
    struct handle *v_addr_region_handle;
    struct handle *channel_handle;
    handle_create(current_process, (object*)process, IR_RIGHT_ALL, &process_handle);
    handle_create(current_process, (object*)root_region, IR_RIGHT_ALL, &v_addr_region_handle);
    handle_create(current_process, (object*)startup_channel, IR_RIGHT_ALL, &channel_handle);

    linked_list_add(&current_process->handle_table, process_handle);
    linked_list_add(&current_process->handle_table, v_addr_region_handle);
    linked_list_add(&current_process->handle_table, channel_handle);

    *process = process_handle->handle_id;
    *v_addr_region = v_addr_region_handle->handle_id;
    *channel = channel_handle->handle_id;
    return IR_OK;
}

/// @brief Create a blank thread primitive. Must be started using `sys_thread_start`
ir_status_t sys_thread_create(ir_handle_t parent_process, ir_handle_t *out) {
    // TODO: Checks
    struct process *process = (struct process*)this_cpu->current_thread->object.parent;
    spinlock_aquire(process->handle_table_lock);

    struct handle *process_handle;
    ir_status_t status = linked_list_find(&process->handle_table, (void*)parent_process, handle_by_id, NULL, (void**)&process_handle);
    if (status != IR_OK) {
        spinlock_release(process->handle_table_lock);
        return IR_ERROR_BAD_HANDLE;
    }

    struct thread *thread;
    thread_create((struct process*)process_handle->object, &thread);

    debug_printf("Created thread %d\n", thread->thread_id);

    struct handle *handle;
    handle_create(process, (object*)thread, IR_RIGHT_ALL, &handle);
    linked_list_add(&process->handle_table, handle);

    spinlock_release(process->handle_table_lock);

    *out = handle->handle_id;
    return IR_OK;
}

/// @brief Begin execution of a thread created with `sys_thread_create`
/// @param thread A new thread that has been adequate but has not yet begun execution
/// @param entry Instruction pointer where the thread will begin execution
/// @param stack_top The threads initial stack pointer
/// @param arg0
/// @return
ir_status_t sys_thread_start(ir_handle_t thread, uintptr_t entry, uintptr_t stack_top, uintptr_t arg0) {
    if (!arch_validate_user_pointer((void*)entry) || !arch_validate_user_pointer((void*)stack_top)) {
        return IR_ERROR_INVALID_ARGUMENTS;
    }

    debug_printf("Thread stack at %#p\n", stack_top);

    struct process *process = (struct process*)this_cpu->current_thread->object.parent;
    spinlock_aquire(process->handle_table_lock);

    struct handle *handle;
    ir_status_t status = linked_list_find(&process->handle_table, (void*)thread, handle_by_id, NULL, (void**)&handle);
    if (status != IR_OK) {
        spinlock_release(process->handle_table_lock);
        return IR_ERROR_BAD_HANDLE;
    }

    thread_start((struct thread*)handle->object, entry, stack_top, arg0);

    spinlock_release(process->handle_table_lock);
    return IR_OK;
}

ir_status_t task_create(struct task *parent) {
    spinlock_aquire(parent->object.lock);
    if (parent->state != ACTIVE) {
        spinlock_release(parent->object.lock);
        return IR_ERROR_BAD_STATE;
    }

    parent->object.references++;

    struct task *new_task = calloc(1, sizeof(struct task));

    if (!new_task) return IR_ERROR_NO_MEMORY;

    new_task->object.type = OBJECT_TYPE_TASK;
    new_task->object.parent = (object*)parent;

    linked_list_add(&parent->object.children, new_task);

    return IR_OK;
}

/// Begin termination of a process
/// Object must be locked before calling
void process_kill_locked(struct process *process, long exit_code) {
    // Remove the process from the parent task
    if (process->object.parent) { // Tasks don't work yet
        spinlock_aquire(process->object.parent->lock);
        linked_list_find_and_remove(&process->object.parent->children, process, NULL, NULL);
        object_decrement_references(process->object.parent);
        spinlock_release(process->object.parent->lock);
    }

    process->state = TERMINATING;
    process->exit_code = exit_code;

    // Make all threads terminate as soon as possible
    // The process will be cleaned up once the last dies
    for (uint i = 0; i < process->object.children.count; i++) {
        struct thread *thread;
        linked_list_get(&process->object.children, i, (void**)&thread);
        thread->state = TERMINATING;
        thread->exit_code = -1ul;
        // Wake up blocking threads to avoid waiting to cleanup the process
        if (thread->blocking_listener) {
            scheduler_unblock_listener(thread->blocking_listener);
        }
        thread->sleeping_until = 0;
    }

    // Alert any processes with handles that we have exited
    object_set_signals(&process->object, process->object.signals | PROCESS_SIGNAL_TERMINATED);

    // Exit code is available, but not all threads will have stopped yet
}

/// Remove the memory backing a process and release its held handles.
/// Transition from `TERMINATING` to `TERMINATED`.
/// Note: Only call when all threads have been terminated
void process_finish_termination(struct process *process) {

    // Release any references this process has to other resources
    struct handle *handle;
    while (IR_OK == linked_list_remove(&process->handle_table, 0, (void**)&handle)) {
        object_decrement_references(handle->object);
        free(handle);
    }

    linked_list_destroy(&process->free_handle_ids);

    // Unmap all of the memory backing this process, even if others have handles to the regions
    v_addr_region_destroy(process->root_v_addr_region);

    // Without executing threads the process is not keeping itself alive
    // The object itself sticks around (inactive) until any handles to it are closed
    // This allows other processes to read its exit code after it terminates
}

/// Use to transition an ending thread from `TERMINATING` to `TERMINATED`
/// NOTE: Should only be called from outside the thread's context, since
///       this destroys its stack
void thread_finish_termination(struct thread *thread) {
    thread->state = TERMINATED;

    object *process = (object *)thread->object.parent;

    spinlock_aquire(process->lock);

    // If this was the last thread the process as a whole has terminated
    if (process->children.count == 1) {
        debug_printf("Last thread exiting, cleaning process\n");
        if (((struct process*)process)->state == ACTIVE) {
            process_kill_locked((struct process*)process, thread->exit_code);
        }
        process_finish_termination((struct process*)process);
    }

    linked_list_find_and_remove(&process->children, thread, NULL, NULL);
    spinlock_release(process->lock);
    object_decrement_references(process);


    v_addr_region_cleanup(thread->kernel_stack);

    // Alert any processes with handles that we have exited
    object_set_signals(&thread->object, thread->object.signals | PROCESS_SIGNAL_TERMINATED);

    // Thread is no longer keeping itself alive by executing.
    // The inactive object will still be kept alive if there are other referrers
    object_decrement_references(&thread->object);
}


/// @brief Process garbage collection handler
void process_cleanup(struct process *process) {
    debug_printf("Freed an exited process\n");
    free(process);
}

/// @brief Thread garbage collection handler
void thread_cleanup(struct thread *thread) {
    debug_printf("Freed an exited thread\n");
    free(thread);
}

ir_status_t sys_process_exit(long exit_code) {
    struct process *this_process = (struct process*)this_cpu->current_thread->object.parent;

    // Cannot kill an already exiting process
    if (this_process->state != ACTIVE) {
        return IR_ERROR_BAD_STATE;
    }

    spinlock_aquire(this_process->object.lock);

    process_kill_locked(this_process, exit_code);

    spinlock_release(this_process->object.lock);

    // The thread will exit when returning from the syscall
    return IR_OK;
}

ir_status_t sys_thread_exit(long exit_code) {
    this_cpu->current_thread->state = TERMINATING;
    this_cpu->current_thread->exit_code = exit_code;
    debug_printf("Thread exiting\n");

    // Thread will discontinue execution upon returning, and `thread_finish_termination` will run next time it is scheduled
    return IR_OK;
}
