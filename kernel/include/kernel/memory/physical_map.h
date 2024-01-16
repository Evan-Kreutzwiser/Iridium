
#ifndef KERNEL_MEMORY_PHYSICAL_MAP_H_
#define KERNEL_MEMORY_PHYSICAL_MAP_H_

// The physical map is a linear mapping of physical memory to kernel space

#include <types.h>
#include <stdint.h>
#include <stddef.h>

extern uintptr_t physical_map_base; // Where all of physical memory is mapped in kernel space
extern size_t physical_map_length; // Size in bytes

// Conversions to and from physical map addresses
#define physical_map_to_p_addr(addr) ((p_addr_t)(addr) - physical_map_base)
#define p_addr_to_physical_map(addr) ((p_addr_t)(addr) + physical_map_base)

#endif // KERNEL_MEMORY_PHYSICAL_MAP_H_
