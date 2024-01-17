
#include "kernel/memory/vmem.h"
#include "kernel/memory/v_addr_region.h"
#include "kernel/memory/physical_map.h"
#include "kernel/memory/init.h"
#include "arch/defines.h"
#include "arch/debug.h"
#include "align.h"
#include <stdbool.h>
#include <stddef.h>

v_addr_t physical_map_base = 0xffff800000000000ul; // The physical map will be at -128 TB, the bottom of kernel space
size_t physical_map_length =        0x8000000000ul; // 512 GB (Hardcoded memory limit)

struct v_addr_region *kernel_region;

address_space kernel_address_space;
bool is_kernel_address_space_set_up = false;

void virtual_memory_init() {
    if (!is_kernel_address_space_set_up) {
        debug_print("WARNING: Must set up kernel address space before calling!\n");
    }

    debug_printf("Creaing root kernel v addr region\n");

    v_addr_region_create_root(get_kernel_address_space(), KERNEL_MEMORY_BASE, KERNEL_MEMORY_LENGTH, &kernel_region);


    extern char _start; // Kernel bounds in virtual memory
    extern char _end;
    uint64_t flags = V_ADDR_REGION_READABLE | V_ADDR_REGION_WRITABLE | V_ADDR_REGION_EXECUTABLE;
    // The pmm has already reserved the pages backing the kernel image,
    // just reserve its virtual address range so nothing else overwrites it
    v_addr_region_create_specific(kernel_region, (v_addr_t)&_start, ROUND_UP_PAGE((v_addr_t)&_end - (v_addr_t)&_start), flags, NULL, NULL);

    debug_printf("Physical map is %#lx bytes long\n", physical_map_length);
    v_addr_region_create_specific(kernel_region, physical_map_base, physical_map_length, V_ADDR_REGION_READABLE | V_ADDR_REGION_WRITABLE, NULL, NULL);
}


void init_kernel_address_space(address_space *addr_space) {
    if (is_kernel_address_space_set_up) {
        debug_print("WARNING: Kernel address space initialized twice!\n");
    }

    // Save the address space created during the arch early init
    kernel_address_space = *addr_space;

    is_kernel_address_space_set_up = true;
}

// Retrieve the kernel address space for modifying kernel mappings
address_space *get_kernel_address_space() {
    return &kernel_address_space;
}
