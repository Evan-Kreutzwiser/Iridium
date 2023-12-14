/// @file kernel/memory/v_addr_region.c
/// @brief Virtual address space management

#include "kernel/memory/v_addr_region.h"
#include "kernel/memory/vm_object.h"
#include "kernel/memory/vmem.h"
#include "kernel/arch/mmu.h"
#include "kernel/arch/arch.h"
#include "kernel/handle.h"
#include "kernel/spinlock.h"
#include "kernel/process.h"
#include "kernel/heap.h"
#include "arch/debug.h"
#include "arch/defines.h"
#include "iridium/types.h"
#include "types.h"
#include "align.h"
#include <stddef.h>

/// @brief Linked list search/sort function for comparing the bases of `v_addr_region`s
/// @return A negative value if `data` is smaller, positive if `target` is larger, or 0 if they are equal
int compare_bases(void *data, void *target) {
    v_addr_t d_base = ((struct v_addr_region*)data)->base;
    v_addr_t t_base = ((struct v_addr_region*)target)->base;
    if (d_base == t_base) return 0;
    else if (d_base < t_base) return -1;
    else return 1;

    // With this line, I ran into a problem with the difference flowing into the sign bit and messing up ordering
    // return ((v_addr_region*)data)->base - ((v_addr_region*)target)->base;
}

/// @brief Create a region representing an entire address space.
/// All operations in an address space will be restricted to the range this region covers,
/// which is helpful for isolating user allocations from the kernel.
/// @param address_space The address space this region will represent
/// @see `arch_mmu_create_address_space`
/// @param base
/// @param length Length of the allowed address space in bytes
/// @param out Output parameter containing the new root region
/// @return `IR_OK` on success, and `out` contains the created region
ir_status_t v_addr_region_create_root(address_space *address_space, v_addr_t base, size_t length, struct v_addr_region **out) {
    struct v_addr_region *region = calloc(1, sizeof(struct v_addr_region));
    if (!region) { return IR_ERROR_NO_MEMORY; }

    // Initialize the region to cover the whole user address space
    region->base = base;
    region->length = length;
    region->containing_address_space = address_space;
    region->object.type = OBJECT_TYPE_V_ADDR_REGION;
    // The caller should add a reference

    *out = region;
    return IR_OK;
}

// Size must be page aligned
// Caller is responsible for checking flags are allowed by parent
ir_status_t v_addr_region_create(struct v_addr_region *parent, size_t length, uint64_t flags,
        struct v_addr_region **out, v_addr_t *address_out) {

    length = ROUND_UP_PAGE(length);
    if (parent->destroyed) {
        return IR_ERROR_BAD_STATE;
    }
    // Search for a free area large enough

    // TODO: This is horribly inefficient
    // Needs a whole different data structure to work well
    v_addr_t previous_end = parent->base;
    bool found = false;
    for (uint i = 0; i < parent->object.children.count; i++) {
        struct v_addr_region *region;
        linked_list_get(&parent->object.children, i, (void**)&region);
        // Check the amount of space inbetween regions
        if (region->base - previous_end >= length) {
            found = true;
            break;
        }
        previous_end = region->base + region->length;
    }

    // Also check if it will fit in after all the existing regions, if theres no space inbetween them
    if (!found && parent->base + parent->length - previous_end < length) {
        return IR_ERROR_NO_MEMORY;
    }

    //debug_printf("Allocated region at %#p in parent %#p\n", previous_end, parent->base);

    parent->object.references++;
    struct v_addr_region *region = calloc(1, sizeof(struct v_addr_region));
    region->object.references = 1;
    region->object.type = OBJECT_TYPE_V_ADDR_REGION;
    region->object.parent = (object*)parent;
    region->destroyed = false;
    region->can_destroy = true;
    region->containing_address_space = parent->containing_address_space;
    region->base = previous_end;
    region->flags = flags;
    region->length = length;

    if (region->base % PAGE_SIZE != 0) {
        debug_printf("WARNING: Creating a non-page-aligned v_addr_region with a base of %#p!\n", region->base);
    }

    //debug_printf("V_ADDR_REGION object created at %#p\n", region);

    // Keep the list sorted to simplify searching through it for gaps
    linked_list_add_sorted(&parent->object.children, compare_bases, region);

    // This pointer can be used to create a hande to the new region
    if (out) *out = region;
    if (address_out) *address_out = region->base;

    return IR_OK;
}

ir_status_t v_addr_region_create_specific(struct v_addr_region *parent, v_addr_t address, size_t length,
        uint64_t flags, struct v_addr_region **out, v_addr_t *address_out) {

    // Round start and end bounds outwards to ensure the region is encompased by whole pages
    p_addr_t old_base = address;
    address = ROUND_DOWN_PAGE(old_base);
    p_addr_t old_end = old_base + length;
    length = ROUND_UP_PAGE(old_end) - address;

    length = ROUND_UP_PAGE(length);
    if (parent->destroyed) {
        return IR_ERROR_BAD_STATE;
    }

    //debug_printf("Allocating %#zx byte region at specific address %#p in parent %#p\n", length, address, parent->base);

    // TODO: Same as above, needs an iterator or different data structure
    v_addr_t start = address, end = start + length;
    for (uint i = 0; i < parent->object.children.count; i++) {
        struct v_addr_region *region;
        linked_list_get(&parent->object.children, i, (void**)&region);

        v_addr_t other_start = region->base, other_end = region->base + region->length;
        // Check the amount of space inbetween regions
        if (start < other_end && end > other_start ) {
            debug_printf("Can't map region from %#p to %#p because it overlaps with a region from %#p to %#p\n", start, end, other_start, other_end);
            return IR_ERROR_NO_MEMORY;
        }
    }

    parent->object.references++;
    struct v_addr_region *region = calloc(1, sizeof(struct v_addr_region));
    region->object.references = 1;
    region->object.type = OBJECT_TYPE_V_ADDR_REGION;
    region->object.parent = (object*)parent;
    region->can_destroy = true;
    region->containing_address_space = parent->containing_address_space;
    region->base = address;
    region->flags = flags;
    region->length = length;

    // Keep the list sorted to simplify searching through it for gaps
    linked_list_add_sorted(&parent->object.children, compare_bases, region);

    // Debugging information, display all of the parent's children
    for (uint i = 0; i < parent->object.children.count; i++) {
        struct v_addr_region *region;
        linked_list_get(&parent->object.children, i, (void**)&region);
    }

    // This pointer can be used to create a hande to the new region
    if (out) *out = region;
    if (address_out) *address_out = address;
    return IR_OK;
}

/// Map a virtual memory object into a virtual address region in a process.
/// The caller should obtain the appropriate locks before calling to
/// maintain reentrancy.
/// If flags contains `IR_MAP_SPECIFIC`, this will either map to that exact address or return an error
/// Only call with locks on the parent `v_addr_region` and `vm_object`
ir_status_t v_addr_region_map_vm_object(struct v_addr_region *parent, uint64_t flags,
        vm_object *vm, struct v_addr_region **out, v_addr_t address, v_addr_t *address_out) {
    // If the caller receives this error they should invalidate their handle to this region
    if (parent->destroyed) {
        return IR_ERROR_BAD_STATE;
    }

    struct v_addr_region *region = NULL;
    ir_status_t status;
    if (flags & V_ADDR_REGION_MAP_SPECIFIC) {
        flags &= ~V_ADDR_REGION_MAP_SPECIFIC;
        status = v_addr_region_create_specific(parent, address, vm->size, flags, &region, &address);
    } else {
        status = v_addr_region_create(parent, vm->size, flags, &region, &address);
    }
    if (status != IR_OK ) return status;

    vm->object.references++;
    parent->object.references++;
    region->vm_object = vm;

    // Create an array of the pages' physical addresses for the paging function

    p_addr_t *physical_addresses = malloc(vm->page_count * sizeof(p_addr_t));
    physical_page_info *page = vm->page_list;
    for (uint i = 0; i < vm->page_count; i++) {
        physical_addresses[i] = page->address;
        page = page->next;
    }

    //debug_printf("Mapping v_addr_region with physical address [0] = %#p\n", physical_addresses[0]);

    ir_status_t result = arch_mmu_map(parent->containing_address_space, address, vm->page_count, physical_addresses, flags);
    free(physical_addresses);
    if (result == IR_ERROR_NO_MEMORY) {
        debug_printf("Mapping failed, removing mapping @ %#p!\n", address);

        // Try to cleanup the failed mappings
        arch_mmu_unmap(region->containing_address_space, address, vm->page_count);
        vm->object.references--;
        // Destroy the region, since the caller cant access memory through it anyway
        linked_list_find_and_remove(&region->object.children, region, NULL, NULL);
        object_decrement_references((object*)parent);
        free(region);

        return IR_ERROR_NO_MEMORY;
    }

    if (out) *out = region;
    if (address_out) *address_out = address;
    return IR_OK;
}

/// @brief Remove a virtual address region
///
/// Recursively removes all child mappings as well
/// The caller should obtain the appropriate locks before calling to
/// maintain reentrancy.
/// Additionally, if a process is using the associated syscall, the
/// handler must ensure that `region->can_destroy` is false and the
/// necessary rights are held.
///
/// The syscall handler is also permitted to redundantly set the
/// destoryed flag in order to minimize time spent locked
/// (All attempted operations will fail if the region is destoryed).
ir_status_t v_addr_region_destroy(struct v_addr_region *region) {

    //debug_printf("Destorying v_addr_region @ %#p\n", (uintptr_t)region);
    //debug_printf("Base: %#p, Length: %#zx\n", region->base, region->length);

    if (linked_list_find_and_remove(&region->object.parent->children, region, compare_bases, NULL) != IR_OK) {
        debug_printf("Failed to remove region from parent!\n");
    }

    // Prevents further operations on the object
    // spinlock_aquire(an object lock)
    region->destroyed = true;
    if (region->object.parent) object_decrement_references(region->object.parent);
    region->object.parent = NULL;

    if (region->vm_object) {
        arch_mmu_unmap(region->containing_address_space, region->base, region->length);
        object_decrement_references((object*)region->vm_object);
    }

    // Recursively free regions
    struct v_addr_region *child;
    while(linked_list_remove(&region->object.children, 0, (void**)&child) == IR_OK) {
        if (!child->destroyed) {
            v_addr_region_destroy(child);
        }
        else {
            debug_print("WARNING: v_addr_region_destory() should not be able to run twice.\n");
        }
    }

    object_decrement_references((object*)region);

    return IR_OK;
}

/// @brief Garbage collection handler for `v_addr_region`s
/// @param region A region with no references
void v_addr_region_cleanup(struct v_addr_region *region) {
    // Very little cleanup is needed, since it won't have any references
    // and when it is destoryed, leaving it unmapped and without a parent or children.
    //debug_printf("Freeing region @ %#p, %#p bytes long\n", region->base, region->length);
    free(region);
}

/// @brief SYSCALL_V_ADDR_REGION_CREATE
/// @param parent A handle to an existing virtual address region
/// @param length Length in bytes of the new region
/// @param flags Access flags. TODO: Verify `flags` is a subset of the parent flags
/// @param region_out Output parameter containing a handle to the new region
/// @param address_out Output parameter containing the region's base address
/// @return `IR_OK` on success, or `IR_ERROR_NO_MEMORY` if the parent region
///         does not have enough space to add the child
ir_status_t sys_v_addr_region_create(ir_handle_t parent, size_t length, uint64_t flags, ir_handle_t *region_out, uintptr_t *address_out) {
    if (!arch_validate_user_pointer(region_out) || !arch_validate_user_pointer(address_out)) {
        return IR_ERROR_INVALID_ARGUMENTS;
    }
    struct process *process = (struct process*)this_cpu->current_thread->object.parent;
    spinlock_aquire(process->handle_table_lock);

    struct handle *parent_handle;
    ir_status_t status = linked_list_find(&process->handle_table, (void*)parent, handle_by_id, NULL, (void**)&parent_handle);
    if (status != IR_OK) {
        spinlock_release(process->handle_table_lock);
        return IR_ERROR_BAD_HANDLE;
    }
    if (parent_handle->object->type != OBJECT_TYPE_V_ADDR_REGION) {
        spinlock_release(process->handle_table_lock);
        return IR_ERROR_WRONG_TYPE;
    }

    struct v_addr_region *parent_region = NULL;

    struct v_addr_region *child;
    v_addr_t address;
    status = v_addr_region_create(parent_region, length, flags, &child, &address);
    if (status != IR_OK) {
        // Child region could not be created
        spinlock_release(process->handle_table_lock);
        return status;
    }

    struct handle *child_handle;
    handle_create(process, (object*)child, IR_RIGHT_ALL, &child_handle);
    linked_list_add(&process->handle_table, child_handle);

    spinlock_release(process->handle_table_lock);
    *region_out = child_handle->handle_id;
    *address_out = address;
    return IR_OK;
}

/// @brief SYSCALL_V_ADDR_REGION_MAP
/// @param parent An existing virtual address region to map the object inside
/// @param vm_object
/// @param flags Memory access flags. TODO: Verify `flags` is a subset of the
///              parent flags and allowed by the vm object
/// @param region_out
/// @param address_out
/// @return `IR_OK` on success, or an error code
ir_status_t sys_v_addr_region_map(ir_handle_t parent, ir_handle_t vm_object, uint64_t flags, ir_handle_t *region_out, uintptr_t *address_out) {
    if (!arch_validate_user_pointer(region_out) || !arch_validate_user_pointer(address_out)) {
        debug_printf("Invalid output pointer %#p or %#p passed to sys_v_addr_region_map\n", region_out, address_out);
        return IR_ERROR_INVALID_ARGUMENTS;
    }

    struct process *process = (struct process*)this_cpu->current_thread->object.parent;
    spinlock_aquire(process->handle_table_lock);

    struct handle *parent_handle, *vm_object_handle;
    ir_status_t status = linked_list_find(&process->handle_table, (void*)parent, handle_by_id, NULL, (void**)&parent_handle);
    ir_status_t status2 = linked_list_find(&process->handle_table, (void*)vm_object, handle_by_id, NULL, (void**)&vm_object_handle);
    if (status != IR_OK || status2 != IR_OK) {
        spinlock_release(process->handle_table_lock);
        return IR_ERROR_BAD_HANDLE;
    }
    if (parent_handle->object->type != OBJECT_TYPE_V_ADDR_REGION || vm_object_handle->object->type != OBJECT_TYPE_VM_OBJECT) {
        spinlock_release(process->handle_table_lock);
        debug_printf("Wrong handle types! Expected 1 and 2, got %d and %d (handles at %#p and %#p)\n", parent_handle->object->type, vm_object_handle->object->type, parent_handle, vm_object_handle);
        debug_printf("Objects at %#p and %#p\n", parent_handle->object, vm_object_handle->object);
        return IR_ERROR_WRONG_TYPE;
    }
    struct v_addr_region *parent_region = (struct v_addr_region*)parent_handle->object;
    struct vm_object *vm = (struct vm_object*)vm_object_handle->object;

    struct v_addr_region *child_region;
    v_addr_t address;
    status = v_addr_region_map_vm_object(parent_region, flags, vm, &child_region, 0, &address);
    if (status != IR_OK) {
        spinlock_release(process->handle_table_lock);
        return status;
    }

    struct handle *child_handle;
    handle_create(process, (object*)child_region, IR_RIGHT_ALL, &child_handle);
    linked_list_add(&process->handle_table, (void**)child_handle);

    spinlock_release(process->handle_table_lock);
    *region_out = child_handle->handle_id;
    *address_out = address;
    return IR_OK;
}
