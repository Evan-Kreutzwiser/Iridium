/// @file kernel/arch/mmu.h
/// @brief Architecture-specific memory mapping functions

#ifndef KERNEL_ARCH_MMU_H_
#define KERNEL_ARCH_MMU_H_

#include <kernel/memory/vmem.h>
#include <types.h>
#include <iridium/types.h>
#include <iridium/errors.h>
#include <stddef.h>
#include <stdint.h>

// Routines for managing memory and address spaces that the architecture must impliment

/// @brief Populate address space data structures to create a new address space
/// @param addr_space A freshly allocated address space that needs initializing
/// @return `IR_OK`
ir_status_t arch_mmu_create_address_space(address_space *addr_space);

/// @brief Cleanup architecture-specific address space data structures
/// @param out
/// @return `IR_OK`
ir_status_t arch_mmu_destory_address_space(address_space *address_space);

/// @brief Switch to a different address space.
/// Kernel memory will remain the same, but userspace
/// mappings will be replaced.
/// @param address_space The address space to enter
void arch_mmu_set_address_space(address_space * address_space);

/// @brief Switch to the kernel address space
/// Because of the virtual to physical address translation
/// `arch_mmu_set_address_space` uses, passing the kernel
/// address space causes a crash. This loads the address space
/// without requiring a switch to page table walking.
void arch_mmu_enter_kernel_address_space();

/// @brief Map some memory into a virtual address soace
/// @param addr_space Address space to map into
/// @param address Virtual address to map
/// @param count Number of memory pages being mapped
/// @param p_addr_list An array of `count` physical addresses
///                    to back the virtual pages
/// @param flags `v_addr_region` access flags
/// @return `IR_OK` on success, or a negative error code
ir_status_t arch_mmu_map(address_space *addr_space, v_addr_t address, size_t count, p_addr_t *p_addr_list, uint64_t flags);
ir_status_t arch_mmu_map_contiguous(address_space *addr_space, v_addr_t address, size_t count, p_addr_t physical_address, uint64_t flags);
ir_status_t arch_mmu_unmap(address_space *addr_space, v_addr_t address, size_t count); // Remove a range of mappings
ir_status_t arch_mmu_protect(address_space *addr_space, v_addr_t address, size_t count, uint flags); // Change the access flags for an existing mapping

#endif // KERNEL_ARCH_MMU_H_
