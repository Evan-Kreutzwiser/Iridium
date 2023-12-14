
#ifndef KERNEL_MEMORY_VM_OBJECT_H_
#define KERNEL_MEMORY_VM_OBJECT_H_

#include <kernel/object.h>
#include <kernel/memory/pmm.h>
#include <iridium/errors.h>
#include <iridium/types.h>
#include <types.h>
#include <stddef.h>
#include <stdbool.h>

/// @brief Kernel object representing a piece of usable memory
/// @see 'v_addr_region' for mapping memory into address spaces
typedef struct vm_object {
    object object;

    /// The list of pages backing this memory object.
    /// @warning Index using `page->next` - NOT `page_list[i]`.
    physical_page_info *page_list;
    /// Number of pages in `page_list`
    size_t page_count;
    /// Size in bytes
    size_t size;

    uint64_t access_flags; // Architecture-specific memory flags
} vm_object;

/// Sets `out` to a pointer to a new virtual memory object representing pages that can be mapped into address spaces
ir_status_t vm_object_create(size_t size, uint64_t flags, vm_object **out);

/// Create a virtual memory object that represents a specific portion of contiguous physical memory
ir_status_t vm_object_create_physical(p_addr_t physical_address, size_t size, uint64_t flags, vm_object **out);

/// Wrap an existing physical memory allocation in a `vm_object`
ir_status_t vm_object_from_page_list(physical_page_info *pages, uint64_t flags, vm_object **out);

/// Free an unused vm_object, releasing the held memory
void vm_object_cleanup(vm_object *vm);

/// @brief SYSCALL_VM_OBJECT_CREATE
ir_status_t sys_vm_object_create(size_t size, uint64_t flags, ir_handle_t *handle);

#endif // KERNEL_MEMORY_VM_OBJECT_H_
