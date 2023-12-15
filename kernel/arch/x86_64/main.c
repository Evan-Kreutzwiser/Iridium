/// @brief Executes x86_64-specific startup routines

#include "arch/x86_64/paging.h"
#include "kernel/arch/arch.h"
#include "kernel/main.h"
#include "kernel/memory/boot_allocator.h"
#include "kernel/process.h"
#include "kernel/memory/init.h"
#include "kernel/memory/pmm.h"
#include "kernel/memory/physical_map.h"
#include "kernel/string.h"
#include "kernel/devices/framebuffer.h"
#include "multiboot/multiboot.h"
#include "arch/defines.h"
#include "arch/x86_64/idt.h"
#include "arch/x86_64/gdt.h"
#include "arch/x86_64/msr.h"
#include "arch/x86_64/acpi.h"
#include "arch/debug.h"
#include "align.h"
#include <cpuid.h>
#include <stdbool.h>
#include <stdnoreturn.h>

#define CPUID_FEATURE_LEAF 1
#define CPUID_EDX_PSE (1 << 3)
#define CPUID_EDX_PAE (1 << 6)
#define CPUID_EDX_PGE (1 << 13)
#define CPUID_EDX_PAT (1 << 16)
#define CPUID_EDX_NX (1 << 20)

#define CPUID_EXTENTED_FEATURE_LEAF 0x80000001
#define CPUID_EXTENDED_EDX_1G (1 << 26)

multiboot_info_t *multiboot_struct;

// Trying to dynamically allocate this before the heap (or even the physical memory
// manager for that matter) is prepared proved problematic. 32 *should* be enough
// for any system, but if its not, this should be expanded.
#define MAX_MEMORY_REGIONS 32
struct physical_region physical_memory_regions[MAX_MEMORY_REGIONS];

/// @brief Pass the computer's physical memory regions on the the physical memory manager.
/// @note This was moved to a seperate function so that `kernel_startup` can create a
///       bootstrap heap that this can dynamically allocate the region list on.
/// @see `memory_regions` for how the array is populated
static void early_get_physical_memory_regions(struct multiboot_info *multiboot_info, struct physical_region **regions, size_t *count) {
    struct multiboot_mmap_entry *memory_map = (void*)(uintptr_t)multiboot_info->mmap_addr;
    size_t regions_count = multiboot_info->mmap_length / sizeof(struct multiboot_mmap_entry); // Number of mmap entries

    debug_printf("%d mmap entries\n", regions_count);

    if (regions_count > MAX_MEMORY_REGIONS) {
        debug_printf("Too many memory regions (%zd)! Truncated to %d.\n", regions_count, MAX_MEMORY_REGIONS);
    }

    // Copy every memory map entry into the kernel's list
    struct multiboot_mmap_entry *entry = memory_map;
    for (uint i = 0; i < regions_count; i++) {
        // Round the regions inwards to full pages, since thats the smallest granuality we can work with.
        p_addr_t old_base = entry->addr;
        p_addr_t old_end = old_base + entry->len;
        physical_memory_regions[i].base = ROUND_UP_PAGE(old_base);
        physical_memory_regions[i].length = ROUND_DOWN_PAGE(old_end) - ROUND_UP_PAGE(old_base);

        // Translate mulitboot memory type to a generic type
        if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
            physical_memory_regions[i].type = REGION_TYPE_AVAILABLE;
        } else if (entry->type == MULTIBOOT_MEMORY_RESERVED) {
            physical_memory_regions[i].type = REGION_TYPE_RESERVED;
        } else if (entry->type == MULTIBOOT_MEMORY_ACPI_RECLAIMABLE) {
            physical_memory_regions[i].type = REGION_TYPE_RECLAIMABLE;
        } else {
            physical_memory_regions[i].type = REGION_TYPE_UNUSABLE;
        }

        debug_printf("Type %d memory region from %#lx to %#lx\n", physical_memory_regions[i].type, physical_memory_regions[i].base, physical_memory_regions[i].base+ physical_memory_regions[i].length);

        entry++;
    }

    *regions = physical_memory_regions;
    *count = regions_count;
}

void arch_main(p_addr_t multiboot_struct_physical) {

    // Get the per-cpu data pointer ready as soon as possible, even if the contained data won't be ready for a while
    arch_set_cpu_local_pointer(&processor_local_data[0]);

    extern struct physical_region *regions_array;
    extern size_t regions_count;

    early_get_physical_memory_regions((struct multiboot_info*)multiboot_struct_physical, &regions_array, &regions_count);

    debug_printf("multiboot struct @ %#p\n", multiboot_struct_physical);

    // Set up exception handlers
    idt_init();
    init_tss();

    unsigned int eax, ebx, ecx, edx;
    __get_cpuid(CPUID_FEATURE_LEAF, &eax, &ebx, &ecx, &edx);
    debug_printf("CPUID feature leaf is %#lx\n", (uint64_t)(ecx) << 32 | edx);
    if (edx & CPUID_EDX_PGE) {
        debug_print("Has PGE\n");
    }
    if (edx & CPUID_EDX_PAT) {
        debug_print("Has PAT\n");
    }
    if (edx & CPUID_EDX_PSE) {
        debug_print("Has PSE\n");
    }
    if (edx & CPUID_EDX_PAE) {
        debug_print("Has PAE\n");
    }
    if (edx & CPUID_EDX_NX) {
        debug_print("Has NX\n");
        // Tell the paging system its allowed to use the feature
        no_execute_supported = true;
    }

    __get_cpuid(CPUID_EXTENTED_FEATURE_LEAF, &eax, &ebx, &ecx, &edx);
    debug_printf("CPUID extended feature leaf is %#lx\n", (uint64_t)(ecx) << 32 | edx);
    if (edx & CPUID_EXTENDED_EDX_1G) {
        debug_print("1G pages supported\n");
    }

    // Create the physical map in kernel space
    paging_init(physical_memory_regions, regions_count);
    // Memory is no longer identity mapped, so access the multiboot info through
    // the physical memory map instead
    multiboot_struct = (multiboot_info_t*)p_addr_to_physical_map(multiboot_struct_physical);

    // Generic startup tasks
    // After this we can use heap methods and memory mapping
    kernel_startup();

    if (multiboot_struct->flags & MULTIBOOT_INFO_FRAMEBUFFER_INFO) {
        if (multiboot_struct->framebuffer_type == MULTIBOOT_FRAMEBUFFER_TYPE_RGB) {
            init_framebuffer(multiboot_struct->framebuffer_addr, multiboot_struct->framebuffer_width,
                multiboot_struct->framebuffer_height, multiboot_struct->framebuffer_pitch, multiboot_struct->framebuffer_bpp);
        }
        else {
            debug_printf("Framebuffer is type %hhd, not RGB!\n", multiboot_struct->framebuffer_type);
        }
    }
    else {
        debug_printf("No framebuffer provided\n");
    }

    //extern v_addr_t framebuffer;
    //memset((void*)framebuffer, 0x128, 100);

    if (!(multiboot_struct->flags & MULTIBOOT_INFO_MODS) || multiboot_struct->mods_count == 0) {
        debug_print("Init ramdisk not provided. Cannot boot.\n");
        arch_pause();
    }

    // Expect the init tarball to be the first module loaded
    const struct multiboot_mod_list *module_list = (struct multiboot_mod_list*)(multiboot_struct->mods_addr + physical_map_base);
    p_addr_t init_module_start = module_list[0].mod_start;
    size_t init_module_length = module_list[0].mod_end - module_list[0].mod_start;

    debug_printf("Initrd.sys @ %#p, %#zx bytes long\n", init_module_start, init_module_length);

    vm_object *multiboot_data;
    ir_status_t status = vm_object_create_physical(multiboot_struct_physical, sizeof(multiboot_struct), VM_READABLE | VM_EXECUTABLE, &multiboot_data);
    if (status != IR_OK) {
        debug_printf("Multiboot struct vm object creation returned status %d\n", status);
    }

    vm_object *initrd_vm;
    status = vm_object_create_physical(init_module_start, init_module_length, VM_READABLE | VM_EXECUTABLE, &initrd_vm);
    if (status != IR_OK) {
        debug_printf("Initrd vm object creation returned status %d\n", status);
    }

    v_addr_t initrd_address;
    status = v_addr_region_map_vm_object(kernel_region, VM_READABLE | VM_WRITABLE, initrd_vm, NULL, 0, &initrd_address);
    if (status != IR_OK) {
        debug_printf("Initrd address region creation returned status %d\n", status);
    }

    //extern int fb_pitch;
    //memset((void*)(framebuffer + (fb_pitch*12)), 0x228, 80);

    // Read acpi tables for hardware information
    // Such as the number of CPUs
    acpi_init();

    // Setup the interrupt controller and enable the timer
    apic_init();

    // Gather processor information and initialize APs
    smp_init();


    //memset((void*)(framebuffer + (fb_pitch*42)), 0x058, 80);

    // Run the init process and other final setup
    kernel_main(initrd_vm, initrd_address);
}

void arch_set_cpu_local_pointer(struct per_cpu_data* cpu_local_data) {
    wrmsr(MSR_GS_BASE, (uint64_t)cpu_local_data);
    wrmsr(MSR_KERNEL_GS_BASE, (uint64_t)cpu_local_data);
    asm volatile ("swapgs");

    debug_printf("GS MSRs set to %#p and %#p\n");
}


void arch_pause() {
    asm volatile ("hlt");
}

/// Prevent interrupts from firing while running important code
void arch_enter_critical() {
    asm volatile("cli");
}

/// Allow interrupts to fire again
void arch_exit_critical() {
    asm volatile("sti");
}

bool arch_validate_user_pointer(void *pointer) {
    // TODO: Might encounter problems with pointers at the very edge of user memory
    return (uintptr_t)pointer <= USER_MEMORY_LENGTH && (uintptr_t)pointer >= PAGE_SIZE;
}

bool arch_is_kernel_pointer(void *pointer) {
    return (uintptr_t)pointer > USER_MEMORY_LENGTH;
}

void arch_set_instruction_pointer(struct registers *registers, uintptr_t pointer) {
    registers->rip = pointer;
}

void arch_set_stack_pointer(struct registers *registers, uintptr_t pointer) {
    registers->rsp = pointer;
}

void arch_set_frame_pointer(struct registers *registers, uintptr_t pointer) {
    registers->rbp = pointer;
}

void arch_set_arg_0(struct registers *registers, uintptr_t arg0) {
    registers->rdi = arg0;
}


/// Port IO for arch-independent ioport objects
void arch_io_output(int port, long value, int word_size) {
    switch (word_size) {
        case SIZE_BYTE:
            asm volatile ("outb %%al, %%dx" : : "d"(port), "a"(value));
            break;
        case SIZE_WORD:
            asm volatile ("outw %%ax, %%dx" : : "d"(port), "a"(value));
            break;
        case SIZE_LONG:
            asm volatile ("outl %%eax, %%dx" : : "d"(port), "a"(value));
            break;
        // Doesn't exist
        // case SIZE_QUAD:
        //     asm volatile ("outq" : : "d"(port), "a"(value));
        //     break;
        default:
            break;
    }
}

/// Port IO for arch-independent ioport objects
long arch_io_input(int port, int word_size) {
    long input = 0;
    switch (word_size) {
        case SIZE_BYTE:
            asm volatile ("inb %%dx, %%al" : "=a"(input) : "d"(port));
            break;
        case SIZE_WORD:
            asm volatile ("inw %%dx, %%ax" : "=a"(input) : "d"(port));
            break;
        case SIZE_LONG:
            asm volatile ("inl %%dx, %%eax" : "=a"(input) : "d"(port));
            break;
        default:
            break;
    }
    return input;
}

void arch_initialize_thread_context(struct registers *context, bool is_kernel) {
    context->cs = is_kernel ? 0x8  : 0x23;
    context->ss = is_kernel ? 0x10 : 0x1b;
    context->rflags = 0x202; // Reserved bit (1) and interrupts enabled
}
