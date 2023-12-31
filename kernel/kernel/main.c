/// @file kernel/main.c
/// @brief Encapsulates generic portion of kernel startup routines

#include "kernel/main.h"
#include "kernel/arch/arch.h"
#include "kernel/arch/mmu.h"
#include "kernel/heap.h"
#include "kernel/process.h"
#include "kernel/string.h"
#include "kernel/memory/init.h"
#include "kernel/memory/v_addr_region.h"
#include "kernel/memory/physical_map.h"
#include "types.h"
#include "arch/defines.h"
#include "arch/address_space.h"
#include "iridium/elf.h"
#include <stddef.h>

#include "arch/debug.h"

/// @brief Run kernel startup routines to prepare memory systems
void kernel_startup() {
    // Prepare a temporary bootstrap heap
    //heap_early_init();

    // Setup physical memory allocation
    // Arch entry code gave this a list of memory regions to use earlier
    // and at this point the physical memory kernel mapping is in place.
    physical_memory_init();

    // Set up the kernel address space object using the previously created mapping
    virtual_memory_init();

    // Transition to a larger heap now that memory can be tracked properly
    //heap_finish_init();

    // Once this has run, cores can ask for their idle threads
    create_idle_process();
}

/// @brief Initialize the scheduler, finalize startup, and begin the init process.
///
/// @param initrd `vm_object` containing the initrd.sys file contents
/// @param initrd_start_address Address of `initrd` mapped into kernel virtual memory
void kernel_main(v_addr_t initrd_start_address) {
    debug_printf("Initrd.sys at %#p\n", initrd_start_address);

    create_idle_process();
    this_cpu->idle_thread = create_idle_thread();

    // Start the init process
    struct process *init_process;
    struct v_addr_region *address_space;
    process_create(&init_process, &address_space);

    Elf64_Ehdr *header = (Elf64_Ehdr*)initrd_start_address;

    if (memcmp(header->e_ident, ELFMAG, SELFMAG) != 0) {

        debug_printf("FATAL: Initrd.sys not an elf file");
        arch_pause();
    }

    Elf64_Phdr *program_header = (Elf64_Phdr*)(initrd_start_address + header->e_phoff);

    for (int i = 0; i < header->e_phnum; i++, program_header++) {

        if (program_header->p_type != PT_LOAD) continue;

        debug_printf("Mapping section with flags %x: %#lx bytes in memory, %#lx on disk\n",
            program_header->p_flags, program_header->p_memsz, program_header->p_filesz);

        uint flags = program_header->p_flags;
        uint v_addr_region_flags = V_ADDR_REGION_MAP_SPECIFIC;

        if (flags & PF_X) v_addr_region_flags |= V_ADDR_REGION_EXECUTABLE;
        if (flags & PF_W) v_addr_region_flags |= V_ADDR_REGION_WRITABLE;
        if (flags & PF_R) v_addr_region_flags |= V_ADDR_REGION_READABLE;

        vm_object *section;
        vm_object_create(program_header->p_memsz, VM_EXECUTABLE | VM_WRITABLE | VM_READABLE, &section);

        // Map the ELF section into the process's memory
        struct v_addr_region *process_region;
        ir_status_t status = v_addr_region_map_vm_object(address_space, v_addr_region_flags,
            section, &process_region, program_header->p_vaddr, NULL);
        if (status != IR_OK) {
            debug_print("Init process section failed to map!\n");
        }

        // Copy the section contents into the process using a temporary kernel mapping
        // to get around not having a vm_object_write function
        struct v_addr_region *kernel_mapping;
        v_addr_t address;
        status = v_addr_region_map_vm_object(kernel_region, V_ADDR_REGION_READABLE | V_ADDR_REGION_WRITABLE, section, &kernel_mapping, 0, &address);
        if (status != IR_OK) {
            debug_print("Section failed to map in kernel for copying\n");
        }

        // Zero the entire section before copying in case p_filesz is less than the size in memory
        // Which would be true for sections like the BSS
        memset((void*)address, 0, program_header->p_memsz);
        memcpy((void*)address, (void*)(initrd_start_address + program_header->p_offset), program_header->p_filesz);

        if (*(long*)address != *(long*)(initrd_start_address + program_header->p_offset)) {
            debug_printf("At start of section, %#p != %#p", *(long*)address, *(long*)(initrd_start_address + program_header->p_offset));
        }

        v_addr_region_destroy(kernel_mapping);
    }

    // Create a stack for the process, and provide it with a blank argc and argv
    vm_object *stack_vm;
    struct v_addr_region *stack;
    v_addr_t stack_address;
    vm_object_create(PAGE_SIZE * 256, VM_WRITABLE | VM_READABLE, &stack_vm); // 1 MB stack
    v_addr_region_map_vm_object(address_space, V_ADDR_REGION_READABLE | V_ADDR_REGION_WRITABLE,
        stack_vm, &stack, 0, &stack_address);
    struct v_addr_region *kernel_mapping;
    v_addr_t stack_kernel_address;
    v_addr_region_map_vm_object(kernel_region, V_ADDR_REGION_READABLE | V_ADDR_REGION_WRITABLE, stack_vm, &kernel_mapping, 0, &stack_kernel_address);
    v_addr_region_destroy(kernel_mapping);

    struct thread *thread;
    ir_status_t status = thread_create(init_process, &thread);
    if (status) {
        debug_printf("Error %d creating init thread\n", status);
    }
    arch_set_instruction_pointer(&thread->context, header->e_entry);
    arch_set_stack_pointer(&thread->context, stack_address + (PAGE_SIZE * 256) -16);
    arch_mmu_set_address_space(&init_process->address_space);

    debug_printf("Kernel stack: %#p\n", stack_kernel_address);

    debug_printf("Entering init process: Jmp to %#p\n", header->e_entry);
    this_cpu->current_thread = thread;
    arch_enter_context(&thread->context);
}
