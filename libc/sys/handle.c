
#include <sys/handle.h>
#include <sys/x86_64/syscall.h>
#include <iridium/types.h>

ir_status_t ir_handle_duplicate(ir_handle_t handle, ir_rights_t new_rights, ir_handle_t *new_handle_out) {
    return _syscall_3(SYSCALL_HANDLE_REPLACE, handle, new_rights, (long)new_handle_out);
}

ir_status_t ir_handle_replace(ir_handle_t handle, ir_rights_t new_rights, ir_handle_t *new_handle_out) {
    return _syscall_3(SYSCALL_HANDLE_REPLACE, handle, new_rights, (long)new_handle_out);
}

ir_status_t ir_handle_close(ir_handle_t handle) {
    return _syscall_1(SYSCALL_HANDLE_CLOSE, handle);
}

void ir_handle_dump(void) {
    _syscall_1(SYSCALL_DEBUG_DUMP_HANDLES, 0);
}
