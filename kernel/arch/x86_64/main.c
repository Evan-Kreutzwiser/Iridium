/// @brief Executes x86_64-specific startup routines

#include "arch/x86_64/paging.h"
#include "kernel/arch/arch.h"
#include "kernel/main.h"
#include "kernel/process.h"
#include "kernel/memory/init.h"
#include "kernel/memory/pmm.h"
#include "kernel/memory/physical_map.h"
#include "kernel/string.h"
#include "kernel/devices/framebuffer.h"
#include "multiboot/multiboot2.h"
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

enum {
  EFI_RESERVED_MEMORY,
  EFI_LOADER_CODE,
  EFI_LOADER_DATA,
  EFI_BOOT_SERVICES_CODE,
  EFI_BOOT_SERVICES_DATA,
  EFI_RUNTIME_SERVICES_CODE,
  EFI_RUNTIME_SERVICES_DATA,
  EFI_CONVENTIONAL_MEMORY,
  EFI_UNUSABLE_MEMORY,
  EFI_ACPI_RECLAIM_MEMORY,
  EFI_ACPI_MEMORY_NVS,
  EFI_MEMORY_MAPPED_IO,
  EFI_MEMORY_MAPPED_IO_PORT_SPACE,
  EFI_PAL_CODE,
  EFI_PERSISTENT_MEMORY,
  EFI_MAX_MEMORY_TYPE
};

struct efi_mmap_entry {
    uint32_t type; // Memory type from above list
    uint32_t padding;
    p_addr_t physical_start;
    v_addr_t virtual_start;
    uint64_t pages_count;
    uint64_t attribute;
};

#define CPUID_FEATURE_LEAF 1
#define CPUID_EDX_PSE (1 << 3)
#define CPUID_EDX_PAE (1 << 6)
#define CPUID_EDX_PGE (1 << 13)
#define CPUID_EDX_PAT (1 << 16)
#define CPUID_EDX_NX (1 << 20)

#define CPUID_EXTENTED_FEATURE_LEAF 0x80000001
#define CPUID_EXTENDED_EDX_1G (1 << 26)

// Trying to dynamically allocate this before the heap (or even the physical memory
// manager for that matter) is prepared proved problematic. 128 *should* be enough
// for any system, even with how much grub splits up the efi memory map, but if its
// not this should be expanded.
#define MAX_MEMORY_REGIONS 128
struct physical_region physical_memory_regions[MAX_MEMORY_REGIONS];

struct arch_reserved_range reserved_memory_regions[1];

// Points to the array of memory ranges arch code wants reserved
extern struct arch_reserved_range *reserved_ranges;
extern size_t reserved_ranges_count;

bool found_framebuffer = false;
bool found_init_module = false;
bool found_memory = false;
bool found_efi_memory = false;
bool found_rsdp = false;

p_addr_t init_module_start;
p_addr_t init_module_end;

/// @brief Pass the computer's physical memory regions on the the physical memory manager.
/// @note This was moved to a seperate function so that `kernel_startup` can create a
///       bootstrap heap that this can dynamically allocate the region list on.
/// @see `memory_regions` for how the array is populated
static void early_get_physical_memory_regions(struct multiboot_tag_mmap *mmap, struct physical_region **regions, size_t *count) {

    // Copy every memory map entry into the kernel's list
    size_t regions_count = 0;
    struct multiboot_mmap_entry *entry;
    for (entry = mmap->entries; ((uintptr_t)entry < (uintptr_t)mmap + mmap->size) && regions_count < MAX_MEMORY_REGIONS; entry++) {
        // Round the regions inwards to full pages, since thats the smallest granuality we can work with.
        p_addr_t old_base = entry->addr;
        p_addr_t old_end = old_base + entry->len;

        struct physical_region *region = &physical_memory_regions[regions_count];
        region->base = ROUND_UP_PAGE(old_base);
        region->length = ROUND_DOWN_PAGE(old_end) - ROUND_UP_PAGE(old_base);

        // Translate mulitboot memory type to a generic type
        if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
            region->type = REGION_TYPE_AVAILABLE;
        } else if (entry->type == MULTIBOOT_MEMORY_RESERVED) {
            region->type = REGION_TYPE_RESERVED;
        } else if (entry->type == MULTIBOOT_MEMORY_ACPI_RECLAIMABLE) {
            region->type = REGION_TYPE_RECLAIMABLE;
        } else {
            region->type = REGION_TYPE_UNUSABLE;
        }

        regions_count++;
    }

    *regions = physical_memory_regions;
    *count = regions_count;
}

void early_get_physical_memory_regions_efi(struct multiboot_tag_efi_mmap *mmap, struct physical_region **regions, size_t *count) {
    debug_printf("Using efi memory map\n");
    size_t regions_count = 0;
    struct efi_mmap_entry *entry;

    p_addr_t previous_end = -1;
    int previous_type = -1;
    struct physical_region *region;
    for (entry = (void*)mmap->efi_mmap;
            ((uintptr_t)entry < (uintptr_t)mmap + mmap->size) && regions_count < MAX_MEMORY_REGIONS;
            entry = (void*)((uintptr_t)entry + mmap->descr_size)
        ) {

        debug_printf("EFI MMAP Entry: %#p, %#p bytes, type %d\n", entry->physical_start, entry->pages_count * PAGE_SIZE, entry->type);

        // Translate mulitboot memory type to a generic type
        int type;
        switch (entry->type) {
            case EFI_LOADER_CODE:
            case EFI_LOADER_DATA:
            case EFI_BOOT_SERVICES_CODE:
            case EFI_BOOT_SERVICES_DATA:
            case EFI_CONVENTIONAL_MEMORY:
                type = REGION_TYPE_AVAILABLE;
                break;
            case EFI_ACPI_RECLAIM_MEMORY:
                type = REGION_TYPE_RECLAIMABLE;
                break;
            default:
                type = REGION_TYPE_RESERVED;
        }

        if (entry->physical_start == previous_end && type == previous_type) {
            region->length += entry->pages_count * PAGE_SIZE;
            debug_printf("Merged with previous\n");
        }
        else {
            region = &physical_memory_regions[regions_count];
            region->base = entry->physical_start;
            region->length = entry->pages_count * PAGE_SIZE;
            region->type = type;
            regions_count++;
        }

        previous_end = region->length + region->base;
        previous_type = type;
    }

    *regions = physical_memory_regions;
    *count = regions_count;

}

void arch_main(p_addr_t multiboot_physical_addr) {

    // Get the per-cpu data pointer ready as soon as possible, even if the contained data won't be ready for a while
    arch_set_cpu_local_pointer(&processor_local_data[0]);

    // Set up exception handlers
    idt_init();
    init_tss();

    unsigned int eax=0, ebx=0, ecx=0, edx=0;
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

    p_addr_t framebuffer_addr = 0;
    int framebuffer_width = 0;
    int framebuffer_height = 0;
    int framebuffer_pitch = 0;
    int framebuffer_bpp = 0;

    uintptr_t rsdp_addr = 0;


    extern struct physical_region *regions_array;
    extern size_t regions_count;

    void *memory_tag = NULL;
    void *efi_memory_tag = NULL;

    struct multiboot_tag *tag = (void*)(multiboot_physical_addr + 8);
    while (tag->type != MULTIBOOT_TAG_TYPE_END) {
        debug_printf("Multiboot tag - Type %d, size %#x\n", tag->type, tag->size);
        switch (tag->type) {
            case MULTIBOOT_TAG_TYPE_MMAP:
                found_memory = true;
                memory_tag = tag;
                break;

            case MULTIBOOT_TAG_TYPE_EFI_MMAP:
                found_efi_memory = true;
                efi_memory_tag = tag;
                break;

            case MULTIBOOT_TAG_TYPE_FRAMEBUFFER:
                struct multiboot_tag_framebuffer_common *framebuffer = (void*)tag;
                if (framebuffer->framebuffer_type == MULTIBOOT_FRAMEBUFFER_TYPE_RGB
                    && framebuffer->framebuffer_width > 0
                    && framebuffer->framebuffer_height > 0
                    && framebuffer->framebuffer_pitch > 0
                    && framebuffer->framebuffer_bpp > 0
                ) {
                    found_framebuffer = true;
                    framebuffer_addr = framebuffer->framebuffer_addr;
                    framebuffer_width = framebuffer->framebuffer_width;
                    framebuffer_height = framebuffer->framebuffer_height;
                    framebuffer_pitch = framebuffer->framebuffer_pitch;
                    framebuffer_bpp = framebuffer->framebuffer_bpp;
                }
                else {
                    debug_printf("Framebuffer is type %hhd, not RGB!\n", framebuffer->framebuffer_type);
                }
                break;

            case MULTIBOOT_TAG_TYPE_MODULE:
                // Expect the init file to be the only module loaded
                if (found_init_module) {
                    debug_printf("WARNING: More than one module loaded. Most recent treated as initrd");
                }
                found_init_module = true;
                struct multiboot_tag_module *module = (void*)tag;
                init_module_start = module->mod_start;
                init_module_end = module->mod_end;
                break;

            case MULTIBOOT_TAG_TYPE_ACPI_OLD:
            case MULTIBOOT_TAG_TYPE_ACPI_NEW:
                // Prefer new ACPI tags
                if (!found_rsdp || tag->type == MULTIBOOT_TAG_TYPE_ACPI_NEW){
                    found_rsdp = true;
                    rsdp_addr = (uintptr_t)&tag[1];
                    debug_printf("Multiboot provided rsdp pointer: %#p\n", rsdp_addr);
                }
                break;
        }

        tag = (void*)ROUND_UP((uintptr_t)tag + tag->size, 8);
    }

    if (found_efi_memory) {
        early_get_physical_memory_regions_efi(efi_memory_tag, &regions_array, &regions_count);
    } else if (found_memory) {
        early_get_physical_memory_regions(memory_tag, &regions_array, &regions_count);
    } else {
        debug_print("Memory map not provided, cannot boot.\n");
        panic(NULL, -1, "Memory map not provided\n");
    }

    debug_printf("%#zd memory regions present\n", regions_count);

    ////////////////////////////
    // After this point the physical map is present and the lower half identity map is gone
    // Create the physical map in kernel space
    paging_init(physical_memory_regions, regions_count);

    // Find regions we want protected and tell the pmm to save them for us while it initalizes
    // such as the initrd file

    if (!found_init_module) {
        debug_print("Init ramdisk not provided. Cannot boot.\n");
        panic(NULL, -1, "Init ramdisk not provided. Cannot boot.\n");
    }

    size_t init_module_length = init_module_end - init_module_start;
    debug_printf("Initrd.sys @ %#p, %#zx bytes long\n", init_module_start, init_module_length);
    reserved_memory_regions[0].base = init_module_start;
    reserved_memory_regions[0].length = init_module_length;

    reserved_ranges = reserved_memory_regions;
    reserved_ranges_count = 1;

    // Generic startup tasks
    // After this we can use heap methods and memory mapping
    kernel_startup();

    if (found_framebuffer) {
        init_framebuffer(framebuffer_addr, framebuffer_width, framebuffer_height,
                         framebuffer_pitch, framebuffer_bpp);
    } else {
        debug_printf("No framebuffer provided\n");
    }

    if (!found_rsdp) {
        panic(NULL, -1, "RSDP not found. Cannot boot.\n");
    }

    // Read acpi tables for hardware information such as the number
    // of CPUs, setup the timer and configure interrupts
    acpi_init(rsdp_addr + physical_map_base);

    framebuffer_print("ACPI setup complete\n");

    // Gather processor information and initialize APs
    smp_init();

    // Run the init process and other final setup
    kernel_main(init_module_start + physical_map_base);
}

void arch_set_cpu_local_pointer(struct per_cpu_data* cpu_local_data) {
    wrmsr(MSR_GS_BASE, (uint64_t)cpu_local_data);
    wrmsr(MSR_KERNEL_GS_BASE, (uint64_t)cpu_local_data);
    asm volatile ("swapgs");
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

struct stack_frame {
    struct stack_frame *rbp;
    uintptr_t rip;
};

void arch_print_stack_trace(struct registers *context) {

    if (context->rip == 0 || context->rbp == 0) {
        framebuffer_printf("Stack trace impossible\n");
        return;
    }

    struct stack_frame *frame = (struct stack_frame*)context->rbp;

    framebuffer_printf("%#.16p", context->rip);
    while ((unsigned long)frame > 0xFFFF800000000000ul)
    {
        framebuffer_printf(" %#.16p", frame->rip);
        frame = frame->rbp;
    }
    framebuffer_print("\n");
}

void arch_print_context_dump(struct registers *context) {
    framebuffer_printf("rip=%#.16p rsp=%#.16p rbp=%#.16p\n\n", context->rip, context->rsp, context->rbp);
    framebuffer_printf("rax=%#.16p rbx=%#.16p rcx=%#.16p rdx=%#.16p\n", context->rax, context->rbx, context->rcx, context->rdx);
    framebuffer_printf("rdi=%#.16p rsi=%#.16p  r8=%#.16p  r9=%#.16p\n", context->rdi, context->rsi, context->r8, context->r9);
    framebuffer_printf("r10=%#.16p r11=%#.16p r12=%#.16p r13=%#.16p\n", context->r10, context->r11, context->r12, context->r13);
    framebuffer_printf("r14=%#.16p r15=%#.16p rflags=%#.16p\n", context->r14, context->r15, context->rflags);
    framebuffer_printf("CS=%#p SS=%#p\n", context->cs, context->ss);
}
