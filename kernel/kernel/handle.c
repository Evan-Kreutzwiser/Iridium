/// @file kernel/handle.c
/// @brief Kernel object handle functions

#include "kernel/handle.h"
#include "kernel/arch/arch.h"
#include "kernel/heap.h"
#include "types.h"
#include "iridium/errors.h"
#include "kernel/object.h"
#include "kernel/process.h"
#include <stdbool.h>
#include "arch/debug.h"

/// @brief Check that a set of rights does not have any permission another set does not
/// @param rights Rights compared against
/// @param requested Rights that should be equal or lesser
/// @return Whether `requested` is a subset of `rights`
static inline bool handle_rights_are_subset(ir_rights_t rights, ir_rights_t requested) {
    // Check that no additional rights are being added
    return (rights | requested) == rights;
}

/// @brief `linked_list` search/compare to function for handle lists
/// @param data Handle ID from a node in the list
/// @param target Target handle ID as a long
/// @return 0 if the IDs match, a negative value if data is lower, or positive if data is higher
int handle_by_id(void *data, void *target) {
    return ((struct handle*)data)->handle_id - (long)target;
}

/// @brief Determine and claim the next valid handle ID
/// @param process Owner of the handle the id will be given to
/// @return A valid handle id for the process
static ir_handle_t handle_get_next_id(struct process *process) {
    ir_handle_t new_id = IR_HANDLE_INVALID;
    ir_status_t result = linked_list_remove(&process->free_handle_ids, 0, (void**)&new_id);
    if (result != IR_OK) {
        atomic_fetch_add(&process->next_handle_id, 1);
        new_id = process->next_handle_id;
    }

    return (ir_handle_t)new_id;
}

/// @brief Create a handle for a kernel object for the current process.
/// NOTE: Does not add to the handle table. The caller must do that after calling.
/// @param process
/// @param object
/// @param rights
/// @param handle
/// @return
ir_status_t handle_create(struct process *process, object *object, ir_rights_t rights, struct handle **handle) {
    object->references++;

    struct handle *new_handle = malloc(sizeof(struct handle));
    new_handle->object = object;
    new_handle->rights = rights;
    new_handle->handle_id = handle_get_next_id(process);
    *handle = new_handle;
    return IR_OK;
}

/// @brief Create a copy of an existing handle
/// @param original The handle being copied
/// @param new_rights Rights used for the new handle.
/// @see `handle_rights_are_subset` to validate new_rights
/// @param new_id ID to assign the new handle
/// @param out Output parameter set to the new handle
/// @return
ir_status_t handle_copy(struct handle *original, ir_rights_t new_rights, uint new_id, struct handle **out) {
    struct handle *new_handle = malloc(sizeof(struct handle));
    if (!new_handle) { return IR_ERROR_NO_MEMORY; }

    // The object has one more referrer keeping it in memory
    object *object = original->object;
    object->references++;

    new_handle->handle_id = new_id;
    new_handle->rights = new_rights;
    new_handle->object = object;

    *out = new_handle;
    return IR_OK;
}

/// @brief SYSCALL_HANDLE_DUPLICATE
///
/// Creates a new copy of a handle with the given rights, and returns the id in `id_out` (in user memory)
int64_t sys_handle_duplicate(long original_id, ir_rights_t new_rights, ir_handle_t *id_out) {
    // As long as the pointer is in userspace its ok
    // Anything else wrong is a logic error and can just crash that process
    if (!arch_validate_user_pointer(id_out)) return IR_ERROR_INVALID_ARGUMENTS;

    // Obtain exclusive access over the process's handle table to avoid other
    // threads deleting the handle out from under us
    struct process *process = (struct process*)this_cpu->current_thread->object.parent;
    spinlock_aquire(process->handle_table_lock);

    struct handle *handle_ptr;
    ir_status_t result = linked_list_find(&process->handle_table, &original_id, handle_by_id, NULL, (void**)&handle_ptr);

    if (result != IR_OK) {
        spinlock_release(process->handle_table_lock);
        return IR_ERROR_BAD_HANDLE;
    }
    // Prevent the caller from gaining new rights
    if (!handle_rights_are_subset(handle_ptr->rights, new_rights)) {
        spinlock_release(process->handle_table_lock);
        return IR_ERROR_BAD_HANDLE;
    }

    ir_handle_t new_handle_id = handle_get_next_id(process);
    handle_copy(handle_ptr, new_rights, new_handle_id, &handle_ptr);
    // TODO: Unhandled path where malloc fails
    linked_list_add(&process->handle_table, handle_ptr);
    spinlock_release(process->handle_table_lock);

    // Give the id the userspace (If it faults accessing this, it is safe to kill the process now)
    *id_out = new_handle_id;
    return IR_OK;
}

/// @brief SYSCALL_HANDLE_REPLACE
/// Replace a handle with a new one that has a subset of the original's rights.
/// @param handle Handle ID to replace. This handle is invalidated after calling.
/// @param new_rights Requested rights for the new handle. Must be a subset of the original handle's rights
/// @param new_handle Output parameter set to the new handle
/// @return `IR_OK` on success, and new_handle contains a valid id. Otherwise returns a negative error code
///         and the given handle remains valid.
ir_status_t sys_handle_replace(ir_handle_t handle, ir_rights_t new_rights, ir_handle_t *new_handle) {
    if (!arch_validate_user_pointer(new_handle)) return IR_ERROR_INVALID_ARGUMENTS;

    // Obtain exclusive access over the process's handle table to avoid other
    // threads deleting the handle out from under us
    struct process *process = (struct process*)this_cpu->current_thread->object.parent;
    spinlock_aquire(process->handle_table_lock);

    struct handle *handle_ptr;
    ir_status_t result = linked_list_find(&process->handle_table, &handle, handle_by_id, NULL, (void**)&handle_ptr);
    if (result != IR_OK) {
        spinlock_release(process->handle_table_lock);
        return IR_ERROR_BAD_HANDLE;
    }
    if (!handle_rights_are_subset(handle_ptr->rights, new_rights)) {
        spinlock_release(process->handle_table_lock);
        return IR_ERROR_INVALID_ARGUMENTS;
    }

    struct handle *new = malloc(sizeof(struct handle));
    handle_copy(handle_ptr, new_rights, handle_get_next_id(process), &new);
    spinlock_release(process->handle_table_lock);

    return IR_OK;
}

/// @brief SYSCALL_HANDLE_CLOSE
/// Close one of the current process's handles
/// @param id Handle id to close
/// @return `IR_OK` on success, or `IR_ERROR_BAD_HANDLE` if the process
///         does not have a handle with the given id
ir_status_t sys_handle_close(ir_handle_t id) {
    struct process *process = (struct process*)this_cpu->current_thread->object.parent;
    spinlock_aquire(process->handle_table_lock);

    struct handle *handle;
    if (linked_list_find_and_remove(&process->handle_table, (void*)id, handle_by_id, (void**)&handle) == IR_OK) {
        spinlock_release(process->handle_table_lock);

        linked_list_add(&process->free_handle_ids, (void*)handle->handle_id);
        object_decrement_references(handle->object);
        free(handle);
        return IR_OK;
    }

    spinlock_release(process->handle_table_lock);
    return IR_ERROR_BAD_HANDLE;
}

ir_status_t sys_handle_dump() {
    struct process *process = (struct process*)this_cpu->current_thread->object.parent;
    spinlock_aquire(process->handle_table_lock);

    struct handle *handle;
    for (uint i = 0; i  < process->handle_table.count; i++) {
        linked_list_get(&process->handle_table, i, (void**)&handle);
        debug_printf("Handle %ld at %#p - object at %#p, rights %#lx\n", handle->handle_id, handle, handle->object, handle->rights);
        debug_printf("Object is type %u\n",  handle->object->type);
    }

    spinlock_release(process->handle_table_lock);
    return IR_OK;

}
