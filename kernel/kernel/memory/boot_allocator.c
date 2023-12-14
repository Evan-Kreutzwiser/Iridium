
// A boot-time memory allocator that takes physical memory off the end
// of where the kernel is loaded

#include <kernel/memory/boot_allocator.h>

#include <kernel/memory/physical_map.h>
#include <types.h>
#include <stdint.h>

extern uintptr_t _end;

// Start from the end of the kernel as defined in linker.ld
p_addr_t boot_allocator_start;
p_addr_t boot_allocator_end;
size_t boot_allocator_size = 0;

// Allocate a chunk of ram after the end of the kernel to the caller
// Returns the physical address, so the caller must translate it to whereever physical memory is mapped at the time
// This uses the memory after the kernel physically, whereas the heap is after the kernel in virtual memory
// and positionaled in completely unrealated physical memory
void *boot_allocator_alloc(size_t length) {

    // If this is the first time this is called, initialize the starting point
    if (boot_allocator_start == 0) {
        boot_allocator_start = (uintptr_t)&_end;
        boot_allocator_end = (uintptr_t)&_end;
    }

    // Limit size to 4 pages, which is how many are reserved during pmm initialization
    if (boot_allocator_size > 4096 * 4) {
        return NULL;
    }

    // Extend the boot allocator's range and give the caller the added area
    p_addr_t pointer = boot_allocator_end;
    boot_allocator_size += length;
    boot_allocator_end += length;

    // Return the physical address of the allocate space
    return (void*)pointer;
}
