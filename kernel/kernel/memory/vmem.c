
#include "kernel/memory/vmem.h"
#include "kernel/memory/v_addr_region.h"
#include "kernel/memory/physical_map.h"
#include "kernel/memory/init.h"
#include "arch/defines.h"
#include <stdbool.h>

#include "arch/debug.h"

struct v_addr_region *kernel_region;

address_space kernel_address_space;
bool is_kernel_address_space_set_up = false;

void virtual_memory_init() {
    if (!is_kernel_address_space_set_up) {
        debug_print("WARNING: Must set up kernel address space before calling!\n");
    }

    vm_object *kernel_vm_object;
    debug_print("Creating kernel region from pages\n");

    vm_object_from_page_list(get_kernel_pages(), VM_READABLE | VM_WRITABLE | VM_EXECUTABLE, &kernel_vm_object);
    // I started writing this and then realized that I don't need to make a vm object,
    // just reserve the region with a `V_ADDR_REGION_MAP_SPECIFIC` v_addr_region in the root.

    debug_printf("Creaing root kernel v addr region\n");

    v_addr_region_create_root(get_kernel_address_space(), KERNEL_MEMORY_BASE, KERNEL_MEMORY_LENGTH, &kernel_region);

    debug_printf("Kernel VMO size: %#zx, from %zd pages\n", kernel_vm_object->size, kernel_vm_object->page_count);

    extern char _start; // Start of kernel in virtual memory
    uint64_t flags = V_ADDR_REGION_READABLE | V_ADDR_REGION_WRITABLE | V_ADDR_REGION_EXECUTABLE;
    v_addr_region_create_specific(kernel_region, (v_addr_t)&_start, kernel_vm_object->size, flags, NULL, NULL);

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
