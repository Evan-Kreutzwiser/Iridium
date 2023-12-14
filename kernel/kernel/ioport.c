/// @file kernel/ioport.c
/// @brief Kernel object allowing usage of the cpu IO ports

#include "kernel/ioport.h"
#include "kernel/arch/arch.h"
#include "kernel/linked_list.h"
#include "kernel/spinlock.h"
#include "kernel/process.h"
#include "kernel/handle.h"
#include "kernel/heap.h"
#include "iridium/errors.h"
#include "iridium/types.h"
#include "types.h"
#include "arch/debug.h"
#include <stdbool.h>

/// Prevent overlapping ranges from being created
lock_t io_space_lock;
/// Sorted list of every port range
linked_list allocated_ranges;

/// @brief `linked_list` search/compare to function for port ranges
/// @param data An IO port range from a node in the list
/// @param target Target base port
/// @return 0 if the IDs match, a negative value if data is lower, or positive if data is higher
int range_by_base(void *data, void *target) {
    return ((struct ioport*)data)->base_port - (long)target;
}

// Places where the kernel reserves io ports (for functionality such as timers)
// should use this function to reserve the port ranges and prevent user applications
// from messing with the hardware. Once reserved, kernel code is free to directly
// use (inline) assembly to access the ports instead of the system calls
ir_status_t ioport_create(uint vector, size_t count, struct ioport **out) {

    // TODO: Make a linked list iterator
    spinlock_aquire(io_space_lock);
    for (uint i = 0; i < allocated_ranges.count; i++) {
        struct ioport *range;
        linked_list_get(&allocated_ranges, i, (void**)&range);
        if (vector + count - 1 >= range->base_port && range->base_port + range->range_length - 1 <= vector) {
            spinlock_release(io_space_lock);
            return IR_ERROR_ALREADY_EXISTS;
        }
    }

    struct ioport *ports = calloc(1, sizeof(struct ioport));
    ports->base_port = vector;
    ports->range_length = count;
    ports->object.type = OBJECT_TYPE_IOPORT;
    linked_list_add_sorted(&allocated_ranges, range_by_base, ports);

    spinlock_release(io_space_lock);

    *out = ports;
    return IR_OK;
}

/// `ioport` garbage collection helper
void ioport_cleanup(struct ioport *range) {
    linked_list_find_and_remove(&allocated_ranges, (void*)(long)range->base_port, range_by_base, NULL);
    free(range);
}

/// SYSCALL_IOPORT_CREATE
ir_status_t sys_ioport_create(unsigned long vector, size_t count, ir_handle_t *out) {
    struct ioport *ports;
    ir_status_t status = ioport_create(vector, count, &ports);
    if (status != IR_OK) return status;

    struct process *process = (struct process*)this_cpu->current_thread->object.parent;
    struct handle *handle;
    status = handle_create(process, (object*)ports, IR_RIGHT_INFO | IR_RIGHT_TRANSFER | IR_RIGHT_DUPLICATE, &handle);
    linked_list_add(&process->handle_table, handle);
    // Does not need the handle table lock, because this does not reference existing handles that must remain alive

    *out = handle->handle_id;
    return IR_OK;
}

/// @brief Send a value to an io port
/// @param ioport IO port or port range
/// @param offset Index into port range
/// @param value Value to output
/// @param word_size Bit size of the value to send
/// @return
ir_status_t sys_ioport_send(ir_handle_t ioport, size_t offset, long value, long word_size) {
    struct process *process = (struct process*)this_cpu->current_thread->object.parent;
    spinlock_aquire(process->handle_table_lock);

    struct handle *handle;
    ir_status_t status = linked_list_find(&process->handle_table, (void*)ioport, handle_by_id, NULL, (void**)&handle);
    if (status != IR_OK) return status;
    struct ioport *ports = (struct ioport*)handle->object;

    if (offset >= ports->range_length) {
        spinlock_release(process->handle_table_lock);
        return IR_ERROR_ACCESS_DENIED;
    }

    arch_io_output(ports->base_port + offset, value, word_size);

    spinlock_release(process->handle_table_lock);
    return IR_OK;
}

/// @brief Read a value from an io port
/// @param ioport IO port or port range
/// @param offset Index into port range
/// @param word_size Bit size of the value to recieve
/// @param out Pointer to location where output should be written. Datatype must match `word_size`.
/// @return
ir_status_t sys_ioport_receive(ir_handle_t ioport, size_t offset, long word_size, long *out) {
    if (!arch_validate_user_pointer(out) || word_size > SIZE_QUAD || word_size < SIZE_BYTE) {
        return IR_ERROR_INVALID_ARGUMENTS;
    }

    struct process *process = (struct process*)this_cpu->current_thread->object.parent;
    spinlock_aquire(process->handle_table_lock);

    struct handle *handle;
    ir_status_t status = linked_list_find(&process->handle_table, (void*)ioport, handle_by_id, NULL, (void**)&handle);
    if (status != IR_OK) return status;
    struct ioport *ports = (struct ioport*)handle->object;

    if (offset >= ports->range_length) {
        spinlock_release(process->handle_table_lock);
        return IR_ERROR_ACCESS_DENIED;
    }

    spinlock_release(process->handle_table_lock);

    *out = arch_io_input(ports->base_port + offset, word_size);

    return IR_OK;
}
