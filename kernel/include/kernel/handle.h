
#ifndef KERNEL_HANDLE_H_
#define KERNEL_HANDLE_H_

#include "types.h"
#include "kernel/object.h"
#include "iridium/types.h"

/// @brief Kernel object handle
///
/// Allows user processes to access kernel objects through id numbers
struct handle {
    ir_handle_t handle_id;
    ir_rights_t rights; // The operations permitted on the refered object
    // Pointer to the object this is a handle for
    // Verify the type is correct before accessing!
    object *object;
};

struct process;

// Linked list search function for using handle IDs
int handle_by_id(void *data, void *target);

ir_status_t handle_create(struct process *process, object *object, ir_rights_t rights, struct handle **handle);
ir_status_t handle_copy(struct handle *original, ir_rights_t new_rights, uint new_id, struct handle **out);

ir_handle_t handle_get_next_id(struct process *process);

/// @brief SYSCALL_HANDLE_DUPLICATE
///
/// Creates a new copy of a handle with the given rights, and returns the id in `id_out` (in user memory)
int64_t sys_handle_duplicate(long original_id, ir_rights_t new_rights, ir_handle_t *id_out);

/// @brief SYSCALL_HANDLE_REPLACE
ir_status_t sys_handle_replace(ir_handle_t handle, ir_rights_t new_rights, ir_handle_t *new_handle);

/// @brief SYSCALL_HANDLE_CLOSE
/// Close one of the current process's handles
/// @param id Handle id to close
/// @return `IR_OK` on success, or `IR_ERROR_BAD_HANDLE` if the process
///         does not have a handle with the given id
ir_status_t sys_handle_close(ir_handle_t id);

ir_status_t sys_handle_dump(void);

#endif // ! KERNEL_HANDLE_H_
