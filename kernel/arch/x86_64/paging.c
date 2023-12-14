/// @file kernel/arch/x86_64/paging.c
/// @brief Memory mapping and page table manipulation

#include "arch/x86_64/paging.h"
#include "arch/address_space.h"
#include "kernel/arch/arch.h"
#include "kernel/arch/mmu.h"
#include "kernel/string.h"
#include "align.h"
#include "kernel/memory/pmm.h"
#include "kernel/memory/physical_map.h"
#include "iridium/errors.h"
#include "iridium/types.h"
#include "types.h"
#include <stddef.h>
#include <stdbool.h>

#include "arch/debug.h"

/// 2 MB
#define LARGE_PAGE_SIZE 0x200000
/// 1 GB
#define GIGABYTE_PAGE_SIZE 0x40000000
/// 512 GB
#define PML4_PAGE_SIZE 0x8000000000

/// The bits of the page entry used for the address
#define PAGE_ADDRESS_MASK 0x000ffffffffff000ul
/// Sign extension sets these bits for kernel memory OR this when extracting address from page tables
#define HIGHER_HALF_BITS 0xffff800000000000ul

// Page table entry flags
#define PAGE_PRESENT 0x1
#define PAGE_WRITABLE (0x1 << 1)
#define PAGE_USER (0x1 << 2)
#define PAGE_WRITE_THROUGH (0x1 << 3)
#define PAGE_CACHE_DISABLE (0x1 << 4)
#define PAGE_ACCESSED (0x1 << 5)
#define PAGE_DIRTY (0x1 << 6)
#define PAGE_LARGE_PAGE (0x1 << 7)
#define PAGE_GLOBAL (0x1 << 8)
#define PAGE_NO_EXECUTE  (0x1ul << 63)

#define IS_PRESENT(entry) ((entry) & PAGE_PRESENT)
#define IS_LARGE_PAGE(entry) ((entry) & PAGE_LARGE_PAGE)

#define PAGE_INDEX_MASK 0x1ff // 9 bit indices in each level of the table

#define PML4_SHIFT 39
#define PML3_SHIFT 30
#define PML2_SHIFT 21
#define PML1_SHIFT 12

#define ADDRESS_PML4_INDEX(x) (((x) >> PML4_SHIFT) & PAGE_INDEX_MASK)
#define ADDRESS_PML3_INDEX(x) (((x) >> PML3_SHIFT) & PAGE_INDEX_MASK)
#define ADDRESS_PML2_INDEX(x) (((x) >> PML2_SHIFT) & PAGE_INDEX_MASK)
#define ADDRESS_PML1_INDEX(x) (((x) >> PML1_SHIFT) & PAGE_INDEX_MASK)

#define INDEX_AT_LEVEL(virtual_address, table_level) (((virtual_address) >> (12 + ((table_level) * 9))) & PAGE_INDEX_MASK)

// Kernel page tables, initialized on entry
// Instead of pml4, pdp, pd, and pt we refer to the levels as pml(#)
page_table_entry kernel_pml4[512] PAGE_ALIGNED;
// All 256 kernel pml3s need to be set ahead of time so kernel mappings can be easily synchronised between processes
page_table_entry kernel_pml3s[256][512] PAGE_ALIGNED;
page_table_entry kernel_pml2[512] PAGE_ALIGNED;
// Once memory allocation is online, the 2 MB pages will be split for fine-tuned access control.
// The physical map uses 1 Gb pages (no pml2) to save space until physical memory is online,
// then finer-grained permissions can be used.

extern uintptr_t _start_physical;

// Private functions
// Internal to this file only
size_t page_size(uint table_level);
uint64_t intermediate_page_flags(uint64_t leaf_flags);
bool maybe_release_frame(page_table_entry *page_frame);


/// Create a new address space for the kernel to reside in,
/// and map all of physical memory into kernel space
void paging_init() {

    // Initialize the pml4 with every kernel pml3 present so the addresses can be copied to any new address spaces
    for (int i = 0; i < 256; i++) {
        kernel_pml4[i + 256] = ((uintptr_t)&kernel_pml3s[i] - KERNEL_VIRTUAL_ADDRESS) | PAGE_PRESENT | PAGE_GLOBAL;
    }

    // Physical mapping to kernel space
    // It is simplest to just map everything (up to 512gb) and protect regions as needed later
    // TODO: Come back to this and take into account memory upper limit
    page_table_entry *physical_map_pml3 = kernel_pml3s[ADDRESS_PML4_INDEX(physical_map_base) - 256];
    p_addr_t physical_address = 0;
    for (int i = 0; i < 512; i++) {
        physical_map_pml3[i] = physical_address | PAGE_LARGE_PAGE | PAGE_PRESENT | PAGE_CACHE_DISABLE | PAGE_WRITABLE | PAGE_NO_EXECUTE | PAGE_GLOBAL;
        physical_address += GIGABYTE_PAGE_SIZE;
    }

    // Add the map to the kernel address space
    physical_address = (uintptr_t)physical_map_pml3 - KERNEL_VIRTUAL_ADDRESS;
    physical_address &= PAGE_ADDRESS_MASK;
    kernel_pml4[ADDRESS_PML4_INDEX(physical_map_base)] = physical_address | PAGE_PRESENT | PAGE_WRITABLE | PAGE_GLOBAL | PAGE_NO_EXECUTE;

    // Map the kernel itself into the address space
    // For now map everything with full permissions
    // Protection can be worked out later when more table levels can be allocated
    page_table_entry *kernel_pml3 = kernel_pml3s[ADDRESS_PML4_INDEX((uintptr_t)&_start_physical + KERNEL_VIRTUAL_ADDRESS) - 256];
    physical_address = (uintptr_t)kernel_pml3 - KERNEL_VIRTUAL_ADDRESS;
    physical_address &= PAGE_ADDRESS_MASK;
    kernel_pml4[ADDRESS_PML4_INDEX((uintptr_t)&_start_physical + KERNEL_VIRTUAL_ADDRESS)] = physical_address | PAGE_PRESENT | PAGE_WRITABLE | PAGE_GLOBAL;

    physical_address = (uintptr_t)&kernel_pml2 - KERNEL_VIRTUAL_ADDRESS;
    physical_address &= PAGE_ADDRESS_MASK;
    kernel_pml3[ADDRESS_PML3_INDEX((uintptr_t)&_start_physical + KERNEL_VIRTUAL_ADDRESS)] = physical_address | PAGE_PRESENT | PAGE_CACHE_DISABLE | PAGE_WRITABLE | PAGE_GLOBAL;

    extern uintptr_t _end_physical;
    uint large_page_count = ROUND_UP((uint64_t)&_end_physical, LARGE_PAGE_SIZE) / LARGE_PAGE_SIZE;
    physical_address = 0;
    for (uint i = 0; i < large_page_count; i++) {
        kernel_pml2[i] = (physical_address & PAGE_ADDRESS_MASK) | PAGE_PRESENT | PAGE_LARGE_PAGE | PAGE_GLOBAL | PAGE_WRITABLE;
        physical_address += LARGE_PAGE_SIZE;
    }

    // Load the address space
    asm volatile ("mov %0, %%cr3" :: "r"(((uintptr_t)&kernel_pml4[0]) - KERNEL_VIRTUAL_ADDRESS) : );
    // At this point, the kernel is running in an address space it has full control over,
    // With all physical memory mapped into the start of kernel space. However, there is little to no
    // Memory protection - all mapped memory (including physical reserved regions) are read/write, and all
    // kernel code/data is executable. Lower memory is no longer mapped.

    // Pass on the table to the virtual memory manager, so kernel memory can be mapped at runtime
    address_space kernel_address_space;
    kernel_address_space.table_base = kernel_pml4;
    init_kernel_address_space(&kernel_address_space);
}

ir_status_t arch_mmu_create_address_space(address_space *addr_space) {
    addr_space->table_base = paging_allocate_table();
    // Copy kernel-space mappings to the new address space
    // The address space can be further populated by mapping objects into the root v_addr_region
    memcpy(addr_space->table_base, kernel_pml4, sizeof(kernel_pml4));
    return IR_OK;
}

void arch_mmu_enter_kernel_address_space() {
    asm volatile ("mov %0, %%cr3" : : "r" ((uint64_t)kernel_pml4 - KERNEL_VIRTUAL_ADDRESS): "memory");
}

void arch_mmu_set_address_space(address_space *address_space) {
    asm volatile ("mov %0, %%cr3" : : "r" (physical_map_to_p_addr(address_space->table_base)): "memory");
}

/// @brief Map a set of pages into an address space
/// @param addr_space The target address space
/// @param address The virtual address to map to
/// @param count The number of pages to map
/// @param p_addr_list Physical addresses of the pages being mapped
/// @param flags Memory access flags
/// @return `IR_OK` on successful mapping, or an error code
ir_status_t arch_mmu_map(address_space *addr_space, v_addr_t address, size_t count, p_addr_t *p_addr_list, uint64_t flags) {
    // TODO: Check that pointers are in kernel space and that flags are valid
    // TODO: Get necessary address space locks

    uint64_t page_flags = PAGE_PRESENT;
    if (flags & V_ADDR_REGION_WRITABLE) page_flags |= PAGE_WRITABLE;
    if (~flags & V_ADDR_REGION_EXECUTABLE) page_flags |= PAGE_NO_EXECUTE;

    if (arch_is_kernel_pointer((void*)address)) page_flags |= PAGE_GLOBAL;
    else page_flags |= PAGE_USER;

    // Map the pages
    page_table_entry *table = addr_space->table_base;
    for (size_t i = 0; i < count; i++) {
        // Map each page individually
        // TODO: Not an efficient way to do this, but its simpler in the short term.
        // Crawls each level from scratch every time
        ir_status_t status = paging_map_page(table, address, *p_addr_list, page_flags);
        if (status != IR_OK ) {
            debug_printf("Paging: Error %d while mapping\n", status);
            return status; // Pass on any errors encountered whhile mapping
        }

        address += PAGE_SIZE;
        p_addr_list++; // Move to the next physiclal page
    }

    return IR_OK;
}

// Map a contigous range of physical memory into the address space
ir_status_t arch_mmu_map_contiguous(address_space *addr_space, v_addr_t address, size_t count, p_addr_t physical_address, uint64_t flags) {
    uint64_t page_flags = PAGE_PRESENT;
    if (flags & V_ADDR_REGION_WRITABLE) page_flags |= PAGE_WRITABLE;
    if (~flags & V_ADDR_REGION_EXECUTABLE) page_flags |= PAGE_NO_EXECUTE;

    // Map the pages
     page_table_entry *table = addr_space->table_base;
    for (size_t i = 0; i < count; i++) {
        // Map each page individually
        // TODO: Not an efficient way to do this, but its simpler in the short term.
        ir_status_t status = paging_map_page(table, address, physical_address, page_flags);
        if (status != IR_OK ) {
            return status; // Pass on any errors encountered whhile mapping
        }

        address += PAGE_SIZE;
        physical_address += PAGE_SIZE;
    }

    return IR_OK;
}

// Change the access flags for an existing mapping
ir_status_t arch_mmu_protect(address_space *addr_space, v_addr_t address, size_t count, uint flags) {

    // Check argument validity
    // TODO: Translate v_addr_region_flags to page flags

    if ( !addr_space || !address ) {
        return IR_ERROR_INVALID_ARGUMENTS; // Null pointers
    }
    if (count == 0) { return IR_OK; }
    // TODO: Check page flags using arch-defined method

    page_table_entry *table = addr_space->table_base;

    for (size_t i = 0; i < count; i++) {
        paging_protect_page(table, address, flags);
        address += PAGE_SIZE;
    }

    return IR_OK;
}

// Remove a range of mappings from an address space
ir_status_t arch_mmu_unmap(address_space *addr_space, v_addr_t address, size_t count) {

    int pages = count / PAGE_SIZE;

    for (int i = 0; i < pages; i++) {
        paging_unmap_page(addr_space->table_base, address);
        address += PAGE_SIZE;
    }

    return IR_OK;
}


// Update the access flags of an existing page
ir_status_t paging_protect_page(page_table_entry *table, v_addr_t virtual_address, uint64_t new_flags) {
    uint64_t intermediate_flags = intermediate_page_flags(new_flags);

    // Work down from the PML4 (top of tables) down to the relevant leaf, updating permissions along the way
    for (uint level = 3; level > 0; level--) {
        uint index = INDEX_AT_LEVEL(virtual_address, level);

        // Break up any large pages in the way
        if (IS_LARGE_PAGE(table[index])) {
            ir_status_t status = paging_split_page(&table[index], level);
            if (status != IR_OK) { return status; } // Bubble any errors up to the caller
        }

        // Abort if the page is not mapped
        if (!IS_PRESENT(table[index])) {
            return IR_ERROR_NOT_FOUND;
        }

        // Update permissions
        table[index] = (table[index] & PAGE_ADDRESS_MASK) | intermediate_flags;
        // Get the next level of the table
        table = (page_table_entry*)((table[index] & PAGE_ADDRESS_MASK) | HIGHER_HALF_BITS);
    }

    // Replace the permissions
    uint index = INDEX_AT_LEVEL(virtual_address, 0);
    p_addr_t physical_address = table[index] & PAGE_ADDRESS_MASK;
    table[index] = physical_address | new_flags;

    asm volatile ("invlpg (%0)" : : "r" (&virtual_address) : "memory");

    return IR_OK;
}

// Map a single physical page to a virtual address space
ir_status_t paging_map_page(page_table_entry *table, v_addr_t virtual_address, p_addr_t physical_address, uint64_t protection_flags) {
    uint64_t intermediate_flags = intermediate_page_flags(protection_flags);

    // Work down from the PML4 (top of tables) down to the relevant leaf, updating permissions along the way
    for (uint level = 3; level > 0; level--) {
        uint index = INDEX_AT_LEVEL(virtual_address, level);

        // Break up any large pages in the way
        if (IS_LARGE_PAGE(table[index])) {
            ir_status_t status = paging_split_page(&table[index], level);
            if (status != IR_OK) { return status; } // Bubble any errors up to the caller
        }

        // Create new page tables where there aren't any yet
        if (!IS_PRESENT(table[index])) {
            p_addr_t new_table = (uint64_t)paging_allocate_table() - physical_map_base;
            if (!new_table) { return IR_ERROR_NO_MEMORY; } // If an intermediate table could not be allocated
            table[index] = new_table & PAGE_ADDRESS_MASK;
        }

        // Update permissions
        table[index] = (table[index] & PAGE_ADDRESS_MASK) | intermediate_flags;

        // Get the next level of the table
        table = (page_table_entry*)((table[index] & PAGE_ADDRESS_MASK) + physical_map_base);
    }

    // Map the physical page to the requested address
    uint index = ADDRESS_PML1_INDEX(virtual_address);
    table[index] = physical_address | protection_flags;

    asm volatile ("invlpg (%0)" : : "r" (&virtual_address) : "memory");
    return IR_OK;
}

/// @brief Remove a page from the memory maps
/// @param table PML4 of the address space being unmapped from
/// @param virtual_address The address to unmap
/// @return `IR_OK` on success, or a negative error code
ir_status_t paging_unmap_page(page_table_entry *table, v_addr_t virtual_address) {

    page_table_entry *levels[4]; // Save pointers to each frame for when we check for empty frames

    // Work down from the PML4 (top of tables) down to the relevant leaf, updating permissions along the way
    for (uint level = 3; level > 0; level--) {
        uint index = INDEX_AT_LEVEL(virtual_address, level);

        // Break up any large pages in the way
        if (IS_LARGE_PAGE(table[index])) {
            ir_status_t status = paging_split_page(&table[index], level);
            if (status != IR_OK) { return status; } // Bubble any errors up to the caller
        }

        // If there isn't a table there then there is nothing for us to do
        if (table[index] == 0) {
            return IR_OK;
        }

        // Save a pointer for when we want to potentially free higher levels
        levels[level] = table;

        // Get the next level of the table
        table = (page_table_entry*)((table[index] & PAGE_ADDRESS_MASK) + physical_map_base);
    }

    levels[0] = table;
    uint index = INDEX_AT_LEVEL(virtual_address, 0);
    table[index] = 0;

    // Check if there are any unused page frames now
    for (int i = 0; i < 3; i++) {

        // Can't free kernel pml3s since the change won't propogate to different processes
        if (i >= 2 && arch_is_kernel_pointer((void*)virtual_address)) { break; }

        if (maybe_release_frame(table)) {
            levels[i+1][INDEX_AT_LEVEL(virtual_address, i+1)] = 0;
        }
        // If we can't free this level we certainly can't free the next one
        else { break; }
    }

    asm volatile ("invlpg (%0)" : : "r" (&virtual_address) : "memory");

    return IR_OK;
}

// Split an entry in a table into a full, lower level table mapping the same memory
ir_status_t paging_split_page(page_table_entry *table_entry, uint table_level) {
    // Create the new table (with smaller individual page sizes for more fine permissions)
    page_table_entry *new_table = paging_allocate_table();
    if (new_table == NULL) {
        return IR_ERROR_NO_MEMORY;
    }

    // Extract the page flags, and if the new pages are the smallest possible size remove the large page flag
    bool new_size_is_large = (table_level - 1) > 0;
    uint64_t flags = (*table_entry) & ~PAGE_ADDRESS_MASK;
    if (!new_size_is_large) { flags &= ~PAGE_LARGE_PAGE; }

    // Fill the table with smaller pages mapping the same area
    // The larger page coveres a contiguous range of physical memory, so break that same range into smaller chunks
    p_addr_t address = (*table_entry) & PAGE_ADDRESS_MASK;

    size_t smaller_page_size = page_size(table_level - 1);
    for (uint i = 0; i < 512; i++) {
        new_table[i] = address | flags;
        address += smaller_page_size;
    }

    // Drop in the new, filled-in table
    // The original entry is no longer a large page, so remove that flag
    uint64_t new_table_physical_address = ((uintptr_t)new_table - physical_map_base) & PAGE_ADDRESS_MASK;
    uint64_t new_flags = intermediate_page_flags(flags) & ~PAGE_LARGE_PAGE;
    *table_entry = new_table_physical_address | new_flags;

    return IR_OK;
}

// Allocate and return new page table for use in the paging system
// The returned pointer is a virtual address in the physical map
// Because the page info isn't stored anywhere, use pmm_page_from_p_addr when it comes time to free it
page_table_entry *paging_allocate_table() {

    // Page tables are the same size as pages, so pop one from the pmm and use it directly
    physical_page_info *page = NULL;
    ir_status_t status = pmm_allocate_page(&page);

    if (status == IR_OK) {
        // Zero the table to prevent malformed entries from appearing in uninitialized areas
        page_table_entry *table = NULL;
        table = (page_table_entry*)p_addr_to_physical_map(page->address);
        for (uint i = 0; i < 512; i++) {
            table[i] = 0;
        }
        // Hand back the table address directly to avoid overhead of tracking pages
        // Instead use pmm_page_from_p_addr to find the page to free later
        return table;
    }

    // The page could not be allocated
    // Should probably cause a panic
    debug_print("FAILED TO ALLOCATE PAGE TABLE\n");
    return NULL;
}

uint64_t intermediate_page_flags(uint64_t leaf_flags) {
    // Strip no execute flag from upper levels of the table.
    // Intel manual Vol. 3A 4-35: a no-execute flag at a higher level will prevent
    // all pages underneath from executing instructions, whereas write access requires
    // all levels to have the writable flag.

    // I am making a guess about the caching flags. If it proves to be a problem I can revisit this.
    return leaf_flags & ~(PAGE_NO_EXECUTE | PAGE_CACHE_DISABLE | PAGE_WRITE_THROUGH);
}

size_t page_size(uint table_level) {
    if (table_level == 0) {
        return PAGE_SIZE; // 4 KB
    }
    else if (table_level == 1) {
        return LARGE_PAGE_SIZE; // 2 MB
    }
    else if (table_level == 2) {
        return GIGABYTE_PAGE_SIZE; // 1 GB
    }
    else if (table_level == 3) {
        return PML4_PAGE_SIZE; // 512 GB
    }
    // Invalid table level
    return -1;
}

/// @brief Free a page frame if all the entries inside are empty
/// @param page_frame Pointer to a page frame that might need to be freed
/// @return Whether the page frame was freed
bool maybe_release_frame(page_table_entry *page_frame) {
    // If there is even a single page still mapped in here we can't remove it
    for (int i = 0; i < 512; i++) {
        // Checking the entry as a whole, not the present flag, so swapped
        // pages don't have their tables removed or access flags forgotten
        if (page_frame[i] != 0) return false;
    }

    physical_page_info *page = pmm_page_from_p_addr(physical_map_to_p_addr(page_frame));
    pmm_free_page(page);
    debug_printf("Released page table @ %#p\n", page_frame);
    return true; // The caller should remove pointers to the frame
}
