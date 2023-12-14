
#ifndef KERNEL_MEMORY_BOOT_ALLOCATOR_H_
#define KERNEL_MEMORY_BOOT_ALLOCATOR_H_

#include <iridium/types.h>
#include <types.h>

#define __need_size_t
#include <stddef.h>

// Returns a pointer to a location in the physical map of size length
void *boot_allocator_alloc(size_t length);

#endif // KERNEL_MEMORY_BOOT_ALLOCATOR_H_
