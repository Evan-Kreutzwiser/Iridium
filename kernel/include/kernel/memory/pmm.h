
#ifndef KERNEL_MEMORY_PMM_H_
#define KERNEL_MEMORY_PMM_H_

#include <iridium/types.h>
#include <types.h>

#include <stddef.h>

#define PAGE_STATE_FREE 0
#define PAGE_STATE_USED 1
/// A page allocated from a reserved region.
/// Helps to keep them off the free list
#define PAGE_STATE_RESERVED_USED 2
/// A page created to back a `pmm_allocate_range` request outside of a memory map region,
/// since sometimes device mmio ranges don't get included in the memory map.
/// Cannot promise exclusive access to these pages
#define PAGE_STATE_USED_OUTSIDE_REGION 3

#define NO_ADDRESS_LIMIT -1

/// @brief Represnts a page of physical memory, and allows building linked lists from pages
typedef struct physical_page_info {
    // Used for making a linked list of free pages
    // Doubly linked to facilitate removing multi-page allocations from the array
    struct physical_page_info* prev;
    struct physical_page_info* next;
    p_addr_t address; // Used when allocating from the linked list
    char state; // Whether the page is free or in use
} physical_page_info;

/// This region system needs a redesign but I'm hoping it works well enough for the time being.
/// At least until I have to start writing drivers and may need to touch reserved memory
typedef enum region_type {
    REGION_TYPE_AVAILABLE,
    REGION_TYPE_RESERVED, /// Some regions marks reserved could include mmio ranges, so record the ranges anyway in case a driver wants it
    REGION_TYPE_RECLAIMABLE, /// Investigate feasibility of an arch_reclaim_memory() function (on x86_64, for acpi tables)
    REGION_TYPE_UNUSABLE /// Regions we know for a facts can't be used, even for mmio
} region_type_t;

/// A range of the physical address space, present in the computer's hardware
struct physical_region {
    p_addr_t base;
    /// Size of the region in bytes
    size_t  length;
    /// The page data backing this region
    physical_page_info *page_array;
    region_type_t type;
};

/// @brief A range of memory requested by architecture entry point to prevent unexpected reuse.
///
/// Exists because the kernel startup function would initialize all memory subsystems at once
/// and make use of dynamic allocation before architecture code gets another change to run, and
/// important data may have been overwritten before it could make the relevant vm_objects.
struct arch_reserved_range {
    p_addr_t base;
    /// Size of the region in bytes
    size_t  length;
    /// Linked list of pages encompasing the range. These pages will not be used for anything
    /// else unless freed by arch code.
    struct physical_page_info *pages;
};

// Main allocation functions
ir_status_t pmm_allocate_page(physical_page_info **page_out);
ir_status_t pmm_allocate_pages(size_t count, physical_page_info **page_list_out);
/// Allocate a set of physically contiguous pages, useful for drivers
ir_status_t pmm_allocate_contiguous(size_t count, p_addr_t physical_upper_limit, physical_page_info **page_list_out);
/// Allocate a specific, predetermined range of memory such as existing mmio regions
ir_status_t pmm_allocate_range(p_addr_t address, size_t length, physical_page_info **page_list_out);

void pmm_free_page(physical_page_info *page);

physical_page_info *pmm_page_from_p_addr(p_addr_t address);

// Memory statistics
size_t pmm_memory_free(void);
size_t pmm_memory_used(void);
size_t pmm_memory_reserved(void);
size_t pmm_memory_upper_limit(void);

#endif // KERNEL_MEMORY_PMM_H_
