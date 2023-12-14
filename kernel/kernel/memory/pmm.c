/// @file kernel/memory/pmm.c
/// Physical memory manager

#include "kernel/memory/pmm.h"
#include "kernel/memory/boot_allocator.h"
#include "kernel/memory/physical_map.h"
#include "kernel/memory/init.h"
#include "kernel/heap.h"
#include "iridium/types.h"
#include "iridium/errors.h"
#include "types.h"
#include "align.h"
#include "kernel/arch/arch.h"
#include <stddef.h>
#include <stdbool.h>

#include "arch/debug.h"

/// @brief Start of the kernel in physical memory
extern uintptr_t _start_physical;
/// @brief End of the kernel in physical memory
extern uintptr_t _end_physical;

physical_page_info *kernel_pages;

// The physical memory manager stores information about each region of memory
// And dedicated space from each region to track its own pages
// Extra data is stored about each page that allows a linked list
// to be built from them, and for cases where linear allocations of longer
// areas are needed, the region's maps can be consulted to find free areas

struct physical_region *regions_array;
size_t regions_count;
size_t initialized_regions;
static char *region_type_strings[] = {
    [REGION_TYPE_AVAILABLE] = "available",
    [REGION_TYPE_RESERVED] = "reserved",
    [REGION_TYPE_RECLAIMABLE] = "reclaimable",
    [REGION_TYPE_UNUSABLE] = "unusable"
};

// A linked list containing every free page of memory in the system
// Serves as a stack for popping physical pages to allocate
// Pages are pushed and popped to the begining stored here
static physical_page_info *free_list = NULL;
static size_t pages_in_free_list = 0; // Used to optimize checks before multi-page allocations

/// @brief Total amount of free memory in the system
size_t memory_free = 0;
/// @brief Total memory used by the kernel and all processes combined
size_t memory_used = 0;

/// @brief Memory marked as reserved or unavailable.
/// This memory cannot be used by conventional processes but
/// range allocations can still be performed from the memory
/// in case mmio regions or ports require it
size_t memory_reserved = 0;

#define PAGE_INDEX_IN_REGION(address, region_base) (((uintptr_t)(address) - (region_base)) / PAGE_SIZE)

// Private functions
// Internal to this file only

static void pmm_init_region(struct physical_region *region);
static void pmm_free_list_push(physical_page_info *page); // Return a page to the free page stack
static physical_page_info *pmm_free_list_pop(); // Grab a free page off the stack
static void pmm_free_list_remove(physical_page_info *page); // Take a specific page out of free lists

/// @brief Initialize the phyiscal memory manager
///
/// This is called to initialize the physical memory allocator once the regions list
/// has been poppulated by the entry code. This function expects the regions' bases,
/// lengths, and types to be set.
void physical_memory_init() {
    // Boot code used the physical address of the regions list, but we can only access it through the kernel space mapping now
    arch_get_physical_memory_regions(&regions_array, &regions_count);

    // Setup each usable region's internal page data array and update memory counters
    // Get all the available regions first, and allocate pages for other regions as required later
    // (Note: Might lead to duplicate region allocation!)
    for (uint i = 0; i < regions_count; i++) {
        struct physical_region *region = &regions_array[i];
        if (region->type == REGION_TYPE_AVAILABLE) {
            debug_printf("Region %u: %#zx bytes @ %#p\n", i, region->length, region->base);
            pmm_init_region(region);
        }
        initialized_regions++;
    }


    debug_printf("Computer has %#zx bytes of available memory\n", memory_free + memory_used);

    // Reserve the pages the kernel resides in
    size_t kernel_size = (uint64_t)&_end_physical - (uint64_t)&_start_physical;
    debug_printf("Allocating %#zx byte kernel range at %#p\n", kernel_size, (uint64_t)&_start_physical);
    pmm_allocate_range((uint64_t)&_start_physical, kernel_size, &kernel_pages);

    size_t page_count = 0;
    physical_page_info *page = kernel_pages;
    while (page != NULL) {
        page_count++;
        page = page->next;
    }
}

/// Setup a physical memory region to preapre it for allocating pages
/// Takes a region struct where the base, legnth, and type are set and prepares
/// allocation data backing the pages
/// @todo This could probably use a cleanup, but it workes
static void pmm_init_region(struct physical_region *region) {
    // Round inwards to make sure we only use full pages
    p_addr_t old_base = region->base;
    region->base = ROUND_UP_PAGE(region->base);
    p_addr_t old_end = old_base + region->length;
    region->length = ROUND_DOWN_PAGE(old_end) - region->base;

    size_t page_count = region->length / PAGE_SIZE;
    size_t page_array_size = ROUND_UP_PAGE(page_count*sizeof(physical_page_info));

    // If the region is too small just mark it as reserved as forget about it
    if (page_array_size >= region->length) {
        region->type = REGION_TYPE_UNUSABLE;
        return; // Trying to set it up could result in accesses to invalid memory
    }

    if (region->type == REGION_TYPE_AVAILABLE) {
        // Back available regions' page data with pages carved from themselves

        // Find and store the begining of the page array
        // The array will be accessible through the physical map in kernel space
        p_addr_t page_array_physical = region->base + region->length - page_array_size;
        physical_page_info *page_array = (void*)p_addr_to_physical_map(page_array_physical);
        region->page_array = page_array;

        // Initialize all the page info in the array
        // And mark the pages used for the array as used
        uint array_start_index = PAGE_INDEX_IN_REGION(page_array_physical, region->base);
        p_addr_t physical_address = region->base;
        for (uint i = 0; i < page_count; i++) {

            physical_page_info *page = &page_array[i];

            page->address = physical_address;

            if (region->base == 0) {
                //debug_printf("Page: %#p %#p, ", page, page->address);
            }
            page->prev = NULL;
            page->next = NULL;
            if (i < array_start_index) { // Free pages
                page->state = PAGE_STATE_FREE;
                pmm_free_list_push(page);
            }
            else { // Pages used to back the page array
                page->state = PAGE_STATE_USED;
                //debug_printf("%#p backing page array, used\n", page->address);
            }

            physical_address += PAGE_SIZE;
        }

        memory_free += region->length - page_array_size;
        memory_used += page_array_size;
    }
    // This method of initializing reserves regions was removed in favor of creating
    // the page objects at runtime when required, which may result in memory leaks
    // unloading drivers but helps reduce the chance of allocating kernel or initrd
    // memory before it can be reserved.
    // TODO: Allow early startup code to specify regions that cannot be overwritten.

    // Although some space will end up wasted, reserved pages need to be
    // kept track of as well so we have something to give vm objects mapping
    // mmio and other driver regions.
    /*else if (region->type == REGION_TYPE_RESERVED) {
        // We don't know what is in reserved regions so the page
        // data needs to be drawn from available regions instead
        debug_printf("Initializing reserved region, need to allocated page array from somewhere else\n");
        physical_page_info *pages_backing_array;
        ir_status_t status = pmm_allocate_contiguous(page_array_size / PAGE_SIZE, -1, &pages_backing_array);
        if (status != IR_OK) {
            debug_printf("WARNING: Could not initialize %#zx byte region at %#p,\n", region->length, region->base);
        }
        region->page_array = (physical_page_info*)p_addr_to_physical_map(pages_backing_array[0].address);

        p_addr_t address = region->base;
        for (uint i = 0; i < page_count; i++) {

            physical_page_info *page = &region->page_array[i];

            page->address = address;
            page->prev = NULL;
            page->next = NULL;

            address += PAGE_SIZE;
            // With seperated logic these pages are kept off the free list
        }

        memory_reserved += region->length;
    }*/
}

/// Allocate a single page of memory off the free page stack
ir_status_t pmm_allocate_page(physical_page_info **page_out) {
    // Pop a page off the free page stack
    physical_page_info *page = pmm_free_list_pop();

    if (page) { // Page successfully popped
        // Mark the page as used and hand it to the caller
        page->state = PAGE_STATE_USED;
        *page_out = page;

        memory_free -= PAGE_SIZE; // Update memory trackers
        memory_used += PAGE_SIZE;

        return IR_OK;
    }

    // Out of memory, no pages in the list
    *page_out = NULL;
    return IR_ERROR_NO_MEMORY;
}

/// Allocate multiple pages (that don't have to be physically contiguous)
/// Returns a physical_page_info linked-list with `count` pages, that the caller can map however they want
ir_status_t pmm_allocate_pages(size_t count, physical_page_info **pages_list_out) {
    // If there aren't enough free pages to allocate
    if (pages_in_free_list < count) {
        return IR_ERROR_NO_MEMORY;
    }

    if (count == 0) {
        *pages_list_out = NULL;
        return IR_OK;
    }

    // Build a linked list of the pages as they're allocated to give the caller
    physical_page_info *first_page = pmm_free_list_pop(); // The start of the linked list, which is returned to the caller
    first_page->state = PAGE_STATE_USED;
    // Attach more pages to form a linked list
    physical_page_info *previous_page = first_page;
    for (uint i = 1; i < count; i++) {
        physical_page_info *page = pmm_free_list_pop();
        previous_page->next = page;
        page->state = PAGE_STATE_USED;
        page->prev = previous_page;
        previous_page = page;
    }

    memory_free -= count * PAGE_SIZE;
    memory_used += count * PAGE_SIZE;

    *pages_list_out = first_page;
    return IR_OK;
}

/// @brief Allocate a set of physically contiguous pages, useful for drivers
/// @see `pmm_allocate_range` for allocating specific physical addresses
/// @param count Number of pages to allocate
/// @param physical_upper_limit The maxmimum physical address for the allocation.
///                             Useful for drivers communicating with hardware that
///                             has less address bits. Set to 0 to disable the limit.
/// @param page_list_out Output parameter set to the allocated pages
/// @return `IR_OK` on success, or `IR_ERROR_NO_MEMORY` If no contiguous
///         regions below `physical_upper_limit` are available
ir_status_t pmm_allocate_contiguous(size_t count, p_addr_t physical_upper_limit, physical_page_info **page_list_out) {

    if (physical_upper_limit == 0) physical_upper_limit = -1;

    struct physical_region *region = NULL;
    // Goes backwards to avoid allocating important space in the first 16MB and around the kernel (Where grub will put the init process)
    // TODO: Figure out a way to make this unnecessary, but continue doing it anyway to safe space for lower limit requests


    for (uint r = 0; r < initialized_regions; r++) { // initialized_regions instead of overall because the init
                                                         // function can call this to get page arrays for reserved regions

        region = &regions_array[r];
        size_t pages = region->length / PAGE_SIZE;

        if (region->type != REGION_TYPE_AVAILABLE || region->base > physical_upper_limit) continue;

        size_t start_index = 0;
        size_t pages_found = 0;
        for (size_t i = 0; i < pages; i++) {
            if (region->page_array[i].state == PAGE_STATE_FREE && region->page_array[i].address < physical_upper_limit) {
                pages_found++;
                if (pages_found == count) { // A large enough area was found
                    physical_page_info *previous = NULL;
                    for (uint p = start_index; p < start_index + count; p++) {
                        physical_page_info *page = &region->page_array[p];
                        page->state = PAGE_STATE_USED;
                        pmm_free_list_remove(page);
                        if (previous) { previous->next = page; }
                        page->prev = previous;
                        previous = page;
                    }
                    debug_printf("PMM: Allocating congituous region %#p-%#p\n", region->page_array[i].address, region->page_array[i].address + count * PAGE_SIZE);

                    memory_free -= count * PAGE_SIZE;
                    memory_used += count * PAGE_SIZE;
                    *page_list_out = &region->page_array[start_index];
                    return IR_OK;
                }
            }
            else {
                start_index = i+1;
                pages_found = 0;
            }
        }
    }

    debug_printf("Failed to allocate group of %zd pages\n", count);

    return IR_ERROR_NO_MEMORY;
}

/// @brief Allocate a range of physical memory at a specific address
/// @see `pmm_allocate_contiguous` for contiguous physical memory where the address is flexible
/// @param address Physical address of the memory range
/// @param length Length of the range in bytes. automatically rounded up to the nearest page.
/// @param page_list_out Output parameter
/// @return `IR_OK` on success, or `IR_ERROR_NO_MEMORY` if the requested memory does not exist,
///         or is partially/completely in use.
ir_status_t pmm_allocate_range(p_addr_t address, size_t length, physical_page_info **page_list_out) {
    if (length == 0) {
        *page_list_out = NULL;
        return IR_OK;
    }
    // Round start and end bounds outwards to ensure the region is encompased by whole pages
    p_addr_t old_base = address;
    address = ROUND_DOWN_PAGE(old_base);
    p_addr_t old_end = old_base + length;
    length = ROUND_UP_PAGE(old_end) - address;

    size_t page_count =  length / PAGE_SIZE;

    debug_printf("PMM: Allocating region %#zx-%#zx\n", address, address + length);

    // Find which region could contain this specific range
    // The range must be fully contained within one region
    struct physical_region *region = NULL;
    for (uint i = 0; i < regions_count; i++) {
        region = &regions_array[i];

        //debug_printf("PMM: %d: %d, %#p, %#p, %d\n", i, (region->base <= address), region->base + region->length, address + length, (region->base + region->length >= address + length));

        // TODO: Fail when partially overlapping instead of creating new `physical_page_info`s
        if ((region->base <= address) && (region->base + region->length >= address + length)
                && region->type == REGION_TYPE_AVAILABLE) {

            debug_printf("PMM: Region to allocate from is %#p - %#p\n", region->base, region->base + region->length);

            uintptr_t start_offset = address - region->base;
            uint start_index = start_offset / PAGE_SIZE;

            physical_page_info *page_array = region->page_array;

            // Check that all the region's pages are free
            bool free = true;
            for (uint i = start_index; i < start_index + page_count; i++) {
                if (page_array[i].state != PAGE_STATE_FREE) {
                    debug_printf("Required page %#p is not free\n", page_array[i].address);
                    free = false;
                    asm volatile ("int $3");
                }
            }

            if (!free) { // Something else is using (part) of the memory range
                debug_print("Found region for allocation but area is not free\n");
                return IR_ERROR_NO_MEMORY;
            }

            // Mark the range's pages as used and take them off the free stack
            // And make a linked list out of the pages to return
            // Though it seems redundant to destroy and rebuild the linked list, it should be done anyway
            // In case the memory had been previously used and freed in the wrong order
            if (region->type == REGION_TYPE_AVAILABLE) {
                physical_page_info *previous = NULL;
                for (uint i = start_index; i < start_index + page_count; i++) {
                    physical_page_info *page = &page_array[i];
                    page->state = PAGE_STATE_USED;
                    pmm_free_list_remove(page);
                    if (previous) { previous->next = page; }
                    page->prev = previous;
                    previous = page;
                }
                previous->next = NULL; // Terminate the list - pmm_free_list_remove doesn't do this
                memory_free -= page_count * PAGE_SIZE;
                memory_used += page_count * PAGE_SIZE;
            }
            // Reserved regions' pages are not kept on the free list
            else if (region->type == REGION_TYPE_RESERVED) {
                physical_page_info *previous = NULL;
                for (uint i = start_index; i < start_index + page_count; i++) {
                    physical_page_info *page = &page_array[i];
                    page->state = PAGE_STATE_RESERVED_USED;
                    if (previous) { previous->next = page; }
                    page->prev = previous;
                    previous = page;
                }
                memory_reserved -= page_count * PAGE_SIZE;
                memory_used += page_count * PAGE_SIZE;
            }

            *page_list_out = &page_array[start_index];
            return IR_OK;
        }
    }

    // If the code reaches here then the requested range is outside of all regions
    // So return out of memory
    debug_printf("Physical range requested at %#p, but does not exist in a range.\n", address);

    physical_page_info *page_array = calloc(page_count, sizeof(physical_page_info));
    page_array[0].address = address;
    page_array[0].state = PAGE_STATE_USED_OUTSIDE_REGION;
    physical_page_info *previous = &page_array[0];
    for (uint i = 1; i < page_count; i++) {
        physical_page_info *page = &page_array[i];
        address += PAGE_SIZE;
        page->state = PAGE_STATE_USED_OUTSIDE_REGION;
        page->prev = previous;
        page->address = address;
        previous->next = page;
        previous = page;
        //debug_printf("PMM: Allocate page %#p for reserved range", address);
    }
    previous->next = NULL;
    memory_used += page_count * PAGE_SIZE;
    *page_list_out = page_array;
    return IR_OK;
}

/// Free a page using it's page info struct, so the page can be reused
void pmm_free_page(physical_page_info *page) {
    char state = page->state;
    page->state = PAGE_STATE_FREE;
    memory_used -= PAGE_SIZE;

    if (state == PAGE_STATE_USED) { // Regular pages
        pmm_free_list_push(page);
        memory_free += PAGE_SIZE;
    }
    else if (state == PAGE_STATE_RESERVED_USED) {
        // Pages from reserved regions are kept off the free list
        memory_reserved += PAGE_SIZE;
    }
    else if (state == PAGE_STATE_USED_OUTSIDE_REGION) {
        // Pages served from areas outside regions (such as certain mmio areas)
        // don't have a place to go when they aren't in use
        //free(page);
    }
}

/// @brief Get the page that corresponds with a given physical address
///
/// @warning Only use this if there is no other way to obtain the address
///          and you are certain it is not in use. This will break linked
///          lists made from page infos.
/// @return The page struct representing the physical address
physical_page_info *pmm_page_from_p_addr(p_addr_t address) {

    // Find which region contains the address
    for (uint i = 0; i < regions_count; i++) {
        struct physical_region *region = &regions_array[i];
        if (region->base <= address && region->base + region->length > address) {
            // Return a reference the requested page
            uint index = PAGE_INDEX_IN_REGION(address, region->base);
            return &region->page_array[index];
        }
    }

    // Invalid address given
    return NULL;
}

/// TODO: Do I actually need this?
physical_page_info *get_kernel_pages() {
    return kernel_pages;
}

size_t pmm_memory_free() {
    return memory_free;
}

size_t pmm_memory_used() {
    return memory_used;
}

size_t pmm_memory_reserved() {
    return memory_reserved;
}

// Internal helper for adding pages to the free pages linked list
static void pmm_free_list_push(physical_page_info *page) {
    physical_page_info *old = free_list;
    page->prev = NULL;
    page->next = old;
    if (old) { // If the stack was completely empty (this can't happen without causing other severe problems)
               // avoid accessing the null pointer
        old->prev = page;
    }
    // This page is the new front of the list
    free_list = page;
    pages_in_free_list++;
}

// Internal helper that pops the first page off the front of the linked list, to be allocated
static physical_page_info *pmm_free_list_pop() {
    physical_page_info *popped_page = free_list;

    // Move the next page to the start of the list
    free_list = popped_page->next;

    // Disconnect the popped page from the rest of the list
    // popped_page->prev is already null from previously being the list's start
    popped_page->next = NULL;
    free_list->prev = NULL;

    pages_in_free_list--;

    return popped_page;
    // The caller is responsible for setting the state to used and updating memory counters
}

// Internal helper to remove a node from the free page linked list
// Used when allocating contiguous ranges or specific addresses,
// to prevent the free list from allocating them again
static void pmm_free_list_remove(physical_page_info *page) {
    // If the page is the start of the list, the list pointer will have to be updated
    if (page == free_list) {
        free_list = page->next;
    }

    // Slice out the page and link the list back together
    physical_page_info *prev = page->prev;
    physical_page_info *next = page->next;
    // Avoid null pointer dereferencing on the borders of the list
    if (prev) {
        prev->next = next;
    }
    if (next) {
        next->prev = prev;
    }
    pages_in_free_list--;
}
