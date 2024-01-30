
#ifndef KERNEL_MEMORY_VMEM_H_
#define KERNEL_MEMORY_VMEM_H_

#include <kernel/memory/vm_object.h>
#include <kernel/linked_list.h>
#include <arch/address_space.h>

#include <stdint.h>

struct v_addr_region;

extern struct v_addr_region *kernel_region;

// Initializes the kernel address space object during early boot
void init_kernel_address_space(address_space *addr_space);

// Get the kernel address space
// Useful for modifying kernel mappings
address_space *get_kernel_address_space(void);

/// Switch to the kernel memory context, with no user thread
/// NOTE: Clears this_cpu->current_thread
void enter_kernel_context();

#endif // KERNEL_MEMORY_VMEM_H_
