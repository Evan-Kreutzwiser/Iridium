#include <sys/vm_object.h>
#include <sys/x86_64/syscall.h>
#include <iridium/syscalls.h>
#include <iridium/types.h>

ir_status_t ir_vm_object_create(size_t size, unsigned long flags, ir_handle_t *handle_out) {
    return _syscall_3(SYSCALL_VM_OBJECT_CREATE, size , flags, (long)handle_out);
}
