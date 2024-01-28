#ifndef KERNEL_MEMORY_V_ADDR_REGION_H_
#define KERNEL_MEMORY_V_ADDR_REGION_H_

#include "kernel/memory/vmem.h"
#include "kernel/memory/vm_object.h"
#include "kernel/object.h"
#include "kernel/linked_list.h"
#include "types.h"
#include <stdbool.h>

/// @brief Kernel object representing ranges of virtual memory
struct v_addr_region {
    object object;

    v_addr_t base;
    size_t length;
    uint64_t flags;
    bool can_destroy; // Root regions cannot be destoryed
    /// If a range is unmapped or otherwise destoryed,
    /// prevent existing handles from operating on it
    bool destroyed;

    /// @brief Helps memory mapping functions find the target addres space to manipulate.
    /// This does not change through the lifetime of the object.
    /// Not relying on the parent process for the pointer allows processes to
    /// modify childrens' address spaces while they hold the appropriate handles
    address_space *containing_address_space;

    /// Used if this region was created by mapping a vm_object into an address region, otherwise null.
    /// This object counts as a reference to prevents the `vm_object` from being freed.
    vm_object *vm_object;

    // Use object's linked list for children
};

/// Creates the root of an address space. The returned object encompases the whole
/// address space and cannot be removed by anything other than the termination of its parent process.
/// Regions can be allocated under this with `v_addr_region_create` or by mapping vm_objects, and this
/// function is the only way to create regions without parents.
ir_status_t v_addr_region_create_root(address_space *address_space, v_addr_t base, size_t length, struct v_addr_region **out);

ir_status_t v_addr_region_create(struct v_addr_region *parent, size_t length, uint64_t flags, struct v_addr_region **out, v_addr_t *address_out);
/// Create a virtual_address_region containing a specific range of the virtual address space
/// `address_out` is set to page-aligned address
ir_status_t v_addr_region_create_specific(struct v_addr_region *parent, v_addr_t address, size_t length, uint64_t flags, struct v_addr_region **out, v_addr_t *address_out);

/// Map a virtual memory object into the address space of virtual address region's host process
ir_status_t v_addr_region_map_vm_object(struct v_addr_region *parent, uint64_t flags, vm_object *vm, struct v_addr_region **out, v_addr_t address, v_addr_t *address_out);

/// Remove a virtual address region
/// Handles will continue to reference it but all operations on it afterwards will fail
void v_addr_region_destroy(struct v_addr_region *region);

/// @brief Garbage collection for virtual address regions
/// @param region An unused `v_addr_region`
void v_addr_region_cleanup(struct v_addr_region *region);

/// @brief SYSCALL_V_ADDR_REGION_CREATE
ir_status_t sys_v_addr_region_create(ir_handle_t parent, size_t length, uint64_t flags, ir_handle_t *region_out, uintptr_t *address_out);

/// @brief SYSCALL_V_ADDR_REGION_MAP
ir_status_t sys_v_addr_region_map(ir_handle_t parent, ir_handle_t vm_object, uint64_t flags, ir_handle_t *region_out, uintptr_t *address_out);

/// @brief SYSCALL_V_ADDR_REGION_DESTROY
ir_status_t sys_v_addr_region_destroy(ir_handle_t region);

#endif // ! KERNEL_MEMORY_V_ADDR_REGION_H_
