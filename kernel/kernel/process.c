/// @file kernel/process.c
/// Process and thread objects and management functions

#include "kernel/process.h"
#include "kernel/cpu_locals.h"
#include "kernel/handle.h"
#include "kernel/scheduler.h"
#include "kernel/memory/v_addr_region.h"
#include "kernel/arch/arch.h"
#include "kernel/arch/mmu.h"
#include "kernel/string.h"
#include "kernel/heap.h"
#include "arch/defines.h"
#include "arch/debug.h"

#define __need_null
#include <stddef.h>

struct per_cpu_data processor_local_data[MAX_CPUS_COUNT];
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

    linked_list_add(&idle_process->threads, idle_thread);
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
/// @return `IR_OK` on success, or `IR_ERROR_NOMEMORY` under out-of-memory conditions
ir_status_t process_create(struct process **process_out, struct v_addr_region **virtual_address_space_out) {

    struct process *process = calloc(1, sizeof(struct process));
    process->object.type = OBJECT_TYPE_PROCESS;
    ir_status_t status = arch_mmu_create_address_space(&process->address_space);
    if (status != IR_OK) {
        free(process);
        return status;
    }
    status = v_addr_region_create_root(&process->address_space, 0, USER_MEMORY_LENGTH, &process->root_v_addr_region);
    if (status != IR_OK) {
        free(process);
        return status;
    }

    debug_printf("Process created at %#p, root v_addr_region at %#p\n", process, process->root_v_addr_region);

    // Each process's first 2 handles are for their own process and address space
    struct handle* process_handle;
    struct handle* v_addr_region_handle;
    handle_create(process, (object*)process, IR_RIGHT_ALL, &process_handle);
    handle_create(process, (object*)process->root_v_addr_region, IR_RIGHT_ALL, &v_addr_region_handle);
    linked_list_add(&process->handle_table, process_handle);
    linked_list_add(&process->handle_table, v_addr_region_handle);

    debug_printf("Process handle id is %ld, root region handle id is %ld\n", process_handle->handle_id, v_addr_region_handle->handle_id);

    *process_out = process;
    *virtual_address_space_out = process->root_v_addr_region;
    return IR_OK;
}

/// @brief Create a new thread
/// @param parent_process The process to spawn a new thread under
/// @param out Output parameter set to the new thread
/// @return `IR_OK` on success, or an error code
ir_status_t thread_create(struct process* parent_process, struct thread **out) {

    struct thread *thread = calloc(1, sizeof(struct thread));

    thread->object.parent = (object*)parent_process;
    arch_initialize_thread_context(&thread->context, false);
    thread->thread_id = next_thread_id;
    next_thread_id++;
    vm_object *kernel_stack_vm;
    v_addr_t stack_base;
    vm_object_create(PER_THREAD_KERNEL_STACK_SIZE, VM_READABLE | VM_WRITABLE, &kernel_stack_vm);
    v_addr_region_map_vm_object(kernel_region, V_ADDR_REGION_READABLE | V_ADDR_REGION_WRITABLE,
        kernel_stack_vm, &thread->kernel_stack, 0, &stack_base);
    // 16 Byte align the top of the stack without entering the next page
    thread->kernel_stack_top = stack_base + PER_THREAD_KERNEL_STACK_SIZE - 16;

    *out = thread;
    return IR_OK;
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

    schedule_thread(thread);

    return IR_OK;
}

/// @brief SYSCALL_PROCESS_CREATE
/// @param process Output parameter set to the new process's handle
/// @param v_addr_region Output parameter giving access to the process's address space
/// @return On success, returns `IR_OK` and process and v_addr_region are valid handle ids.
ir_status_t sys_process_create(ir_handle_t *process, ir_handle_t *v_addr_region) {
    if (!arch_validate_user_pointer(process) || !arch_validate_user_pointer(v_addr_region)) {
        return IR_ERROR_INVALID_ARGUMENTS;
    }

    struct process *current_process = (struct process*)this_cpu->current_thread->object.parent;

    struct process *new_process;
    struct v_addr_region *root_region;
    process_create(&new_process, &root_region);
    struct handle* process_handle;
    struct handle* v_addr_region_handle;
    handle_create(current_process, (object*)process, IR_RIGHT_ALL, &process_handle);
    handle_create(current_process, (object*)root_region, IR_RIGHT_ALL, &v_addr_region_handle);

    linked_list_add(&current_process->handle_table, process_handle);
    linked_list_add(&current_process->handle_table, v_addr_region_handle);

    *process = process_handle->handle_id;
    *v_addr_region = v_addr_region_handle->handle_id;
    return IR_OK;
}

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

/// Save an interrupt context to be reentered later
void thread_save_context(struct registers *context, struct thread *thread) {
    memcpy(&thread->context, context, sizeof(struct registers));
}
