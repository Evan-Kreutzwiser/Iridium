/// @file kernel/memory/vm_object.c

#include "kernel/memory/vm_object.h"
#include "kernel/memory/pmm.h"
#include "kernel/handle.h"
#include "kernel/spinlock.h"
#include "kernel/process.h"
#include "kernel/arch/arch.h"
#include "kernel/heap.h"
#include "iridium/types.h"
#include "iridium/errors.h"
#include "types.h"
#include "align.h"
#include "arch/defines.h"

#include <arch/debug.h>

ir_status_t vm_object_create(size_t size, uint64_t flags, vm_object **out) {
    // Round the size up to a multiple of the architecture's page size
    size = ROUND_UP_PAGE(size);

    vm_object *vm_obj = calloc(1, sizeof(vm_object));

    vm_obj->object.type = OBJECT_TYPE_VM_OBJECT;
    vm_obj->size = size;
    vm_obj->page_count = size / PAGE_SIZE;
    vm_obj->access_flags = flags;

    // Obtain some physical memory pages to back the object
    ir_status_t status = pmm_allocate_pages(vm_obj->page_count, &vm_obj->page_list);

    if (status != IR_OK) {
        free(vm_obj);
        return status;
    }

    *out = vm_obj;
    return IR_OK;
}

/// Create a memory object representing a contiguous range of physical memory
ir_status_t vm_object_create_physical(p_addr_t physical_address, size_t size, uint64_t flags, vm_object **out) {
    // Round start and end bounds outwards to ensure the region is encompased by whole pages
    p_addr_t old_base = physical_address;
    physical_address = ROUND_DOWN_PAGE(physical_address);
    p_addr_t old_end = old_base + size;
    size = ROUND_UP_PAGE(old_end) - physical_address;

    vm_object *vm_obj = calloc(1, sizeof(vm_object));

    // Reserve a specific region of phyiscal memory
    vm_obj->size = size;
    vm_obj->page_count = size / PAGE_SIZE;
    vm_obj->access_flags = flags;
    vm_obj->object.type = OBJECT_TYPE_VM_OBJECT;

    ir_status_t status = pmm_allocate_range(physical_address, size, &vm_obj->page_list);
    if (status == IR_OK) {
        *out = vm_obj;
        return IR_OK;
    }

    // If the range could not be reserved discard the object
    *out = NULL;
    free(vm_obj);
    return status;
}

/// @brief Encapsulate a list of pre-allocated physical pages in a virtual memory object
/// @param pages List of pages retrieved from a previous call to a function in the pmm_allocate family
/// @param flags Restrictions on how the region can be used
/// @param vm Output parameter set to the new `vm_object`
/// @return `IR_OK` on success, or `IR_ERROR_NO_MEMORY`
ir_status_t vm_object_from_page_list(physical_page_info *pages, uint64_t flags, vm_object **out) {
    vm_object *vm_obj = calloc(1, sizeof(vm_object));
    if (!vm_obj) return IR_ERROR_NO_MEMORY;

    size_t page_count = 0;
    physical_page_info *page = pages;
    while (page != NULL) {
        page_count++;
        page = page->next;
    }

    // Reserve a specific region of phyiscal memory
    vm_obj->size = page_count * PAGE_SIZE;
    vm_obj->page_count = page_count;
    vm_obj->access_flags = flags;
    vm_obj->page_list = pages;
    vm_obj->object.type = OBJECT_TYPE_VM_OBJECT;

    *out = vm_obj;
    return IR_OK;
}

/// @brief Called when a `vm_object` is no longer referred to anywhere and will be removed
/// @param vm The virtual memory object being freed
/// @see `object_decrement_references`
void vm_object_cleanup(vm_object *vm) {
    physical_page_info *page = vm->page_list;
    while (page->next != NULL) {
        physical_page_info *next = page->next;
        pmm_free_page(page);
        page = next;
    }

    free(vm);
}

/// @brief SYSCALL_VM_OBJECT_CREATE
/// Creates a new vm_object
/// @param size Bytes of memory to allocaate. Rounded upwards to a multiple of the system's page size
/// @param flags Memory access flags
/// @param handle Output parameter containing a handle for the `vm_object`
/// @return `IR_OK` on success, or `IR_ERROR_NO_MEMORY` under out-of-memory conditions
ir_status_t sys_vm_object_create(size_t size, uint64_t flags, ir_handle_t *handle_out) {
    if (!arch_validate_user_pointer(handle_out)) {
        debug_printf("Invalid output pointer %#p passed to sys_vm_object_create\n", handle_out);
        return IR_ERROR_INVALID_ARGUMENTS;
    }

    struct process *process = (struct process*)this_cpu->current_thread->object.parent;
    // This does not have to obtain the handle table lock because it does not access
    // any of the process's objects or remove handles, which means it will not
    // interfere with other system calls

    vm_object *vm_object;
    ir_status_t status = vm_object_create(size, flags, &vm_object);
    if (status != IR_OK) return status;

    struct handle *object_handle;
    ir_rights_t rights = IR_RIGHT_ALL;
    if (~flags & VM_EXECUTABLE) {
        rights &= ~IR_RIGHT_EXECUTE;
    }

    status = handle_create(process, (object*)vm_object, rights, &object_handle);
    if (status != IR_OK) {
        debug_printf("Error %d creating vmo handle\n", status);
        return status;
    }

    debug_printf("VMO handle created! id = %ld, object = %#p\n", object_handle->handle_id, object_handle->object);

    status = linked_list_add(&process->handle_table, object_handle);
    if (status != IR_OK) { return status; }

    *handle_out = object_handle->handle_id;
    return IR_OK;
}
