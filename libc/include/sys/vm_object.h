
#ifndef _LIBC_VM_OBJECT_H_
#define _LIBC_VM_OBJECT_H_

#ifdef __cplusplus
extern "C" {
#endif

#define __need_size_t
#include <stddef.h>
#include <iridium/types.h>

// Wrappers for raw vm object system calls

ir_status_t ir_vm_object_create(size_t size, unsigned long flags, ir_handle_t *handle_out);

// More calls will be added in the future, such as duplicating and potentially resizing

#ifdef __cplusplus
}
#endif

#endif // _LIBC_VM_OBJECT_H_
