/// @file arch/x86_64/acpi.c
/// @brief Managing multiprocessor systems, and APIC interrupt setup

#include <arch/x86_64/acpi.h>
#include <arch/x86_64/msr.h>
#include <arch/x86_64/pit.h>
#include <arch/x86_64/idt.h>
#include <arch/registers.h>
#include <kernel/main.h>
#include <kernel/string.h>
#include <kernel/devices/framebuffer.h>
#include <kernel/memory/physical_map.h>
#include <kernel/memory/v_addr_region.h>
#include <kernel/memory/vm_object.h>
#include <kernel/process.h>
#include <kernel/scheduler.h>
#include <kernel/heap.h>
#include <kernel/arch/mmu.h>
#include <kernel/arch/arch.h>
#include <align.h>
#include <stddef.h>
#include <stdbool.h>
#include <cpuid.h>

#include <arch/debug.h>

// APIC register offsets
#define APIC_LAPIC_ID 0x20
#define APIC_LAPIC_VERSION 0x30
#define APIC_TASK_PRIORITY 0x80
#define APIC_ARBITRATION_PRIORITY 0x90
#define APIC_PROCESSOR_PRIORITY 0xA0
#define APIC_EOI 0xB0
#define APIC_REMOTE_READ 0xC0
#define APIC_LOGICAL_DESTINATION 0xD0
#define APIC_DESTINATION_FORMAT 0xE0
#define APIC_SPURIOUS_INT_VECTOR 0xF0
#define APIC_IN_SERVICE 0x100
#define APIC_TRIGGER_MODE 0x180
#define APIC_INTERRUPT_REQUEST 0x200
#define APIC_ERROR_STATUS 0x280
#define APIC_INTERRUPT_COMMAND 0x300
#define APIC_LVT_TIMER 0x320
#define APIC_LVT_THERMAL_SENSOR 0x330
#define APIC_LVT_PERF_MONITOR_COUNTERS 0x340
#define APIC_LVT_LINT0 0x350
#define APIC_LVT_LINT1 0x360
#define APIC_LVT_ERROR 0x370
#define APIC_TIMER_INITIAL_COUNT 0x380
#define APIC_TIMER_CURRENT_COUNT 0x390
#define APIC_TIMER_DIVIDE 0x3E0

#define APIC_LVT_INT_MASK (1 << 16)
#define APIC_TIMER_MODE_PERIODIC (1 << 17)

#define IO_APIC_VERSION_REGISTER 1
#define IO_APIC_REDIRECTION_TABLE_BASE 0x10

#define HPET_CAPABILITIES_AND_ID 0x0
#define HPET_CONFIGURATION 0x10
#define HPET_INTERRUPT_STATUS 0x20
#define HPET_MAIN_COUNTER 0xF0

const struct acpi_rsdp_v2 *rsdp = NULL;
const struct acpi_madt *madt = NULL;
const struct acpi_header *fadt = NULL;
const struct acpi_header *srat = NULL;
const struct acpi_header *ssdt = NULL;
const struct acpi_hpet *hpet = NULL;

static uintptr_t local_apic_mmio_base;
static vm_object *local_apic_mmio_vm_object;

static uintptr_t hpet_mmio_base;
static vm_object *hpet_mmio_vm_object;

struct io_apic_info {
    uintptr_t address;
    uint base;
    uint entry_count;
};

static struct v_addr_region* io_apic_mmio_v_addr_region;
static struct v_addr_region* hpet_mmio_v_addr_region;
static v_addr_t io_apic_mmio_address;
static struct io_apic_info *io_apics;
static int io_apic_count;

static void record_acpi_table_address(const struct acpi_header* table) {
    if (strncmp(table->signature, "APIC", 4) == 0) {
        madt = (struct acpi_madt*)table;
    }
    else if (strncmp(table->signature, "FACP", 4) == 0) {
        fadt = table;
    }
    else if (strncmp(table->signature, "SRAT", 4) == 0) {
        srat = table;
    }
    else if (strncmp(table->signature, "SSDT", 4) == 0) {
        ssdt = table;
    }
    else if (strncmp(table->signature, "HPET", 4) == 0) {
        hpet = (struct acpi_hpet*)table;
    }
}

extern page_table_entry kernel_pml4[512];

static void find_acpi_tables() {
    if (rsdp->revision < 2) {
        // Use RSDT for 32 bit addresses
        struct rsdt *rsdt = (struct rsdt*)(rsdp->rsdt_address + physical_map_base);
        struct acpi_header *header = &(rsdt->header);
        if (!acpi_checksum(header)) {
            debug_print("WARNING: RSDT checksum is invalid!\n");
        }
        debug_printf("RSDT @ %#p\n", rsdt);
        // Iterate through all the tables saving addresses for the ones we need
        int count = (rsdt->header.length - sizeof(struct acpi_header)) / 4;
        for (int i = 0; i < count; i++) {
            const struct acpi_header *table = (struct acpi_header *)(rsdt->sdt_pointers[i] + physical_map_base);
            if (acpi_checksum(table)) {
                debug_printf("Found \"%c%c%c%c\" @ %#p, %#zx bytes\n", table->signature[0], table->signature[1], table->signature[2], table->signature[3], table, table->length);
                //framebuffer_printf("Found \"%c%c%c%c\" @ %#p, %#zx bytes\n", table->signature[0], table->signature[1], table->signature[2], table->signature[3], table, table->length);
                record_acpi_table_address(table);
            } else { debug_print("An ACPI table failed the checksum\n"); }
        }
    } else {
        // Version 2, use XSDT for 64 bit addresses
        const struct xsdt *xsdt = (struct xsdt*)(rsdp->xsdt_address + physical_map_base);
        if (!acpi_checksum(&xsdt->header)) {
            debug_print("WARNING: XSDT checksum is invalid!\n");
        }

        debug_printf("XSDT @ %#p\n", xsdt);
        // Iterate through all the tables saving addresses for the ones we need
        const int count = (xsdt->header.length - sizeof(struct acpi_header)) / 8;
        for (int i = 0; i < count; i++) {
            struct acpi_header *table = (void*)(xsdt->sdt_pointers[i] + physical_map_base);
            if (acpi_checksum(table)) {
                debug_printf("Found \"%c%c%c%c\" @ %#p, %#zx bytes\n", table->signature[0], table->signature[1], table->signature[2], table->signature[3], table, table->length);
                //framebuffer_printf("Found \"%c%c%c%c\" @ %#p, %#zx bytes\n", table->signature[0], table->signature[1], table->signature[2], table->signature[3], table, table->length);
                record_acpi_table_address(table);
            } else { debug_print("An ACPI table failed the checksum\n"); }
        }
    }
}

/// @brief Write a value to an IO APIC's register using the window mechanism
/// @param io_apic_base Base address of the IO APIC's mmio
/// @param offset Which register to write to
/// @param value The value to write to the register
void io_apic_write(uintptr_t io_apic_base, int offset, uint32_t value) {
    *(uint32_t volatile*)(io_apic_base) = offset;
    *(uint32_t volatile*)(io_apic_base + 0x10) = value;
}

/// @brief Read one of an IO APIC's registers using the window mechanism
/// @param io_apic_base Base address of the IO APIC's mmio
/// @param offset Which register to read from
/// @return The register's value
uint32_t io_apic_read(uintptr_t io_apic_base, int offset) {
    *(uint32_t volatile*)(io_apic_base) = offset;
    return *(uint32_t volatile*)(io_apic_base + 0x10);
}

/// @brief
/// @param interrupt IO APIC interrupt line
/// @param gsi Interrupt number in the CPU's interrupt table
void io_apic_interrupt_redirection(int interrupt, int gsi) {

    struct io_apic_info *info = NULL;
    for (int i = 0; i < io_apic_count; i++) {
        if (io_apics[i].base <= (uint)interrupt && io_apics[i].base + io_apics[i].entry_count >= (uint)interrupt) {
            info = &io_apics[i];
            break;
        }
    }

    if (!info) {
        debug_printf("Could not redirect interrupt %d - no io apic manages that line\n", interrupt);
        framebuffer_printf("Could not redirect interrupt %d - no io apic manages that line\n", interrupt);
        return;
    }

    int io_offset = (interrupt - info->base) * 2 + IO_APIC_REDIRECTION_TABLE_BASE;

    io_apic_write(info->address, io_offset, gsi);
}

/// Read a value from the apic's mmio registers
static inline long apic_io_input(int register_offset) {
    return *(volatile int*)(local_apic_mmio_base + register_offset);
}

/// Write a value to the apic's mmio registers
static inline void apic_io_output(int register_offset, int value) {
    *(volatile int*)(local_apic_mmio_base + register_offset) = value;
}

void apic_send_eoi() {
    // Must write a 0 to signal EOI, other values may generate a fault
    apic_io_output(APIC_EOI, 0);
}

// Enabled the APIC interrupt controller
void apic_init() {
    // TODO: Map mmio as strong uncachable?

    // Set the enable flag in the APIC's msr
    const long apic_base = rdmsr(MSR_APIC_BASE);
    debug_printf("APIC base: %#p - enabled: %d\n", apic_base, (apic_base & MSR_APIC_BASE_ENABLE) != 0);
    wrmsr(MSR_APIC_BASE, (apic_base & ~0xfffful) | MSR_APIC_BASE_ENABLE);
    // Enable this cpu's local apic
    // Bit 8 is the enable  flag
    // The APIC requires a Spurious interrupt vector to enable, we use vector 0xff
    apic_io_output(APIC_SPURIOUS_INT_VECTOR, apic_io_input(APIC_SPURIOUS_INT_VECTOR) | 0x1ff);
}

// Initialize the cpu's local apic timer
void timer_init() {


    if (hpet) {
        int hpet_irq = -1;
        framebuffer_print("Setting up HPET\n");
        extern char irq_pit;
        idt_set_entry(33, 0x8, (uintptr_t)&irq_pit, IDT_GATE_INTERRUPT, 0); // Sets the oneshot fired variable
        vm_object_create_physical(hpet->base_address.address, PAGE_SIZE, VM_MMIO_FLAGS, &hpet_mmio_vm_object);
        v_addr_region_map_vm_object(kernel_region, V_ADDR_REGION_DISABLE_CACHE | V_ADDR_REGION_READABLE | V_ADDR_REGION_WRITABLE, hpet_mmio_vm_object, &hpet_mmio_v_addr_region, 0, &hpet_mmio_base);

        bool supports_legacy_remapping = *(uint64_t*)hpet_mmio_base >> 15 & 1;
        bool supports_64_bit = *(uint64_t*)hpet_mmio_base >> 13 & 1;
        int comparator_count = *(uint64_t*)hpet_mmio_base >> 8 & 0b11111;
        framebuffer_printf("HPET: Supports legacy remapping: %d, comparator count: %d, supports 64 bit: %d\n",
            supports_legacy_remapping, comparator_count + 1, supports_64_bit);

        // Get a bitmap of every irq line the first comparator supports
        framebuffer_print("Supported IRQ #s: ");
        uint32_t supported_interrupts = *(uint64_t*)(hpet_mmio_base + 0x100) >> 32;
        for (int i = 0; i < 31; i++) {
            if (supported_interrupts & 1) {
                framebuffer_printf("%d ", i);
                hpet_irq = i;
            }
            supported_interrupts >>= 1;
        }
        framebuffer_print("\n");

        if (hpet_irq < 0) {
            panic(NULL, -1, "HPET cannot be assigned an IRQ\n");
        }

        // Set timer comparator registers
        uint64_t period = (*(uint64_t*)(hpet_mmio_base) >> 32) & 0xffffffff;
        uint64_t interval = ROUND_UP(10000000000000, period); // Wait 10 milliseconds (1+E13)(counter is in femtoseconds)
        framebuffer_printf("Timer period is %zu femtoseconds - target wait is %zu\n", period, interval);
        uint64_t config_register = *(uint64_t*)(hpet_mmio_base + HPET_CONFIGURATION);
        *(uint64_t*)(hpet_mmio_base + HPET_CONFIGURATION) = (config_register & ~0b10) | 1; // Enable HPET (without legacy interrupt mapping)

        uint64_t main_counter = *(uint64_t*)(hpet_mmio_base + 0xF0);
        *(uint64_t*)(hpet_mmio_base + 0x148) = main_counter + interval;

        // Set timer configuration register
        io_apic_interrupt_redirection(hpet_irq, 33);
        framebuffer_printf("Mapped HPET irq to %d\n", hpet_irq);
        *(uint64_t*)(hpet_mmio_base + 0x140) = (hpet_irq << 9) | (1 << 2);


        if ((*(uint64_t*)(hpet_mmio_base + 0x140) >> 9 & 0b11111) != hpet_irq) {
            framebuffer_printf("HPET didn't accept destination, used %d instead\n", *(uint64_t*)(hpet_mmio_base + 0x100) >> 9 & 0b11111);
        }
    } else {
        pit_init();
    }

    // Set timer divier to 16
    apic_io_output(APIC_TIMER_DIVIDE, 3);
    // Reset counter
    apic_io_output(APIC_TIMER_INITIAL_COUNT, 0xffffffff);

    if (hpet) {
        framebuffer_print("About to fire HPET\n");
        arch_exit_critical();

        extern volatile bool oneshot_triggered;
        while (!oneshot_triggered) {
            arch_pause();
        }

        arch_enter_critical();

    } else {
        framebuffer_print("About to use legacy timer\n");
        // Enable interrupts so we can receive the timer signal
        arch_exit_critical();
        // Sleep for 10ms
        pit_one_shot(10);
        // Disable interrupts until we're executing processes to avoid the
        // timer going off while we're setting up the kernek
        arch_enter_critical();
    }

    // Measure how many ticks passed during that sleep
    apic_io_output(APIC_LVT_TIMER, APIC_LVT_INT_MASK);
    unsigned long elapsed_ticks = 0xffffffff - apic_io_input(APIC_TIMER_CURRENT_COUNT);
    debug_printf("APIC timer has %lu ticks in 10ms\n", elapsed_ticks);
    framebuffer_printf("APIC timer has %lu ticks in 10ms\n", elapsed_ticks);

    // Start the timer to fire every 10ms on interrupt 32
    // Now that we know how many ticks adds up to 10ms
    apic_io_output(APIC_LVT_TIMER, 32 | APIC_TIMER_MODE_PERIODIC);
    apic_io_output(APIC_TIMER_DIVIDE, 3);
    apic_io_output(APIC_TIMER_INITIAL_COUNT, elapsed_ticks);
}

uint64_t timer_ticks = 0; // 100 = 1 second
void timer_fired(struct registers* context) {
    timer_ticks++;

    if (timer_ticks % 4 == 0) {
        //arch_enter_critical();
        // When the task resumes, return from this function
        struct thread *thread = this_cpu->current_thread;
        //memcpy(&thread->context, context, sizeof(registers));

        // Something is wrong with the copying and the registers get corrupted
        //memcpy(&thread->context, context, sizeof(struct registers));

        arch_save_context(&thread->context);
        arch_set_instruction_pointer(&thread->context, (uint64_t)arch_leave_function);

        switch_task(true);
    }
}

void acpi_init(v_addr_t rsdp_addr) {

    rsdp = (struct acpi_rsdp_v2*)rsdp_addr;
    find_acpi_tables();

    framebuffer_print("Found ACPI tables\n");

    debug_printf("Creating mmio vmo @ %#p\n", (uint64_t)madt->local_apic_address);
    ir_status_t status = vm_object_create_physical(madt->local_apic_address, PAGE_SIZE, VM_MMIO_FLAGS, &local_apic_mmio_vm_object);
    if (status != IR_OK) {
        debug_printf("lapic mmio reserving failed with code %d\n", status);
        panic(NULL, -1, "Local apic MMIO reserving failed.");
    }
    status = v_addr_region_map_vm_object(kernel_region, V_ADDR_REGION_READABLE | V_ADDR_REGION_WRITABLE | V_ADDR_REGION_DISABLE_CACHE, local_apic_mmio_vm_object, NULL, 0, &local_apic_mmio_base);
    if (status != IR_OK) {
        debug_printf("lapic mmio mapping failed with code %d\n", status);
        panic(NULL, -1, "Local apic MMIO mapping failed.");
    }

    debug_printf("mmio mapped to %#p\n", local_apic_mmio_base);

    // Setup the interrupt controller
    apic_init();

    framebuffer_print("Interrupt controller setup\n");

    // Count the number of processor local APIC's, which will
    // equal the number of cpu cores in the system
    int count = 0;
    struct madt_entry_header* entry = (void*)&madt[1];
    uintptr_t end = (uintptr_t)madt + madt->header.length;
    while ((uintptr_t)entry < end) {
        if (entry->type == ACPI_MADT_ENTRY_PROCESSOR_LOCAL_APIC) {
            if (((struct processor_local_apic*)entry)->flags != 0) {
                count++;
            }
        }
        else if (entry->type == ACPI_MADT_ENTRY_IO_APIC) {
            io_apic_count++;
        }
        // Select next entry
        entry = (void*)((uintptr_t)entry + entry->length);
    }

    cpu_count = count;
    debug_printf("Computer has %d CPUs\n", count);

    framebuffer_printf("Computer has %d CPUs\n", count);


    paging_print_tables(get_kernel_address_space()->table_base, local_apic_mmio_base);

    paging_print_tables(get_kernel_address_space()->table_base, 0xffff800000000000);

    // Get this core's apic id from the mmio registers
    uint8_t bsp_apic_id = *((uint32_t*)(local_apic_mmio_base + 0x20));


    io_apics = calloc(io_apic_count, sizeof(struct io_apic_info));

    // Save data about each CPU's local APIC
    // And record io apic information
    int cpu = 0;
    int io_apic_count = 0;
    int pit_entry_number = 0; // The io apic entry that the legacy pit maps to
    entry = (void*)&madt[1];
    end = (uintptr_t)madt + madt->header.length;
    while ((uintptr_t)entry < end) {
        if (entry->type == ACPI_MADT_ENTRY_PROCESSOR_LOCAL_APIC) {
            struct processor_local_apic *lapic = (void*)entry;
            processor_local_data[cpu].core_id = cpu;
            processor_local_data[cpu].arch.local_apic_id = lapic->apic_id;

            if (lapic->apic_id == bsp_apic_id && cpu != 0) {
                debug_print("BSP lapic not first in list!\n");
                framebuffer_print("BSP lapic not first in list!\n");
            }
            if (lapic->flags != 0) {
                cpu++;
            }
        }
        else if (entry->type == ACPI_MADT_ENTRY_IO_APIC) {
            struct io_apic *io_apic = (void*)entry;
            // The MADT contains a 32 bit physical address
            if (!io_apic_mmio_v_addr_region) {
                struct vm_object *mmio_vm;
                vm_object_create_physical(io_apic->address, PAGE_SIZE, VM_WRITABLE | VM_WRITABLE | VM_DISABLE_CACHING, &mmio_vm);
                v_addr_region_map_vm_object(kernel_region, V_ADDR_REGION_WRITABLE | V_ADDR_REGION_READABLE, mmio_vm, &io_apic_mmio_v_addr_region, 0, &io_apic_mmio_address);
            }
            io_apics[io_apic_count].address = io_apic_mmio_address;
            io_apics[io_apic_count].base = io_apic->global_interrupt_base;
            // Bits 16-23 of the io apic's version register contain the number of interrupt redirection entries
            io_apics[io_apic_count].entry_count = (io_apic_read(io_apics[io_apic_count].address, IO_APIC_VERSION_REGISTER) >> 16) & 0Xff;
            debug_printf("Found IO APIC at physical address %#p, manages %d lines starting at %d\n", io_apic->address, io_apics[io_apic_count].entry_count, io_apics[io_apic_count].base);
            framebuffer_printf("Found IO APIC at physical address %#p, manages %d lines starting at %d\n", io_apic->address, io_apics[io_apic_count].entry_count, io_apics[io_apic_count].base);
            io_apic_count++;
        }
        // Also figure out which io apic entry the PIT maps to
        // We need to know in order to calibrate the local apic timer
        else if (entry->type == ACPI_MADT_ENTRY_INTTERUPT_SOURCE_OVERRIDE) {
            struct interrupt_source_override *override = (void*)entry;
            if (override->irq_source == 0) {
                pit_entry_number = override->global_system_interrupt;
            }

            debug_printf("Int source override %#x -> %#x\n", override->irq_source, override->global_system_interrupt);
            framebuffer_printf("Int source override %#x -> %#x\n", override->irq_source, override->global_system_interrupt);
        }
        else if (entry->type == ACPI_MADT_ENTRY_LOCAL_APIC_NMI) {
            struct local_apic_nmi *nmi = (void*)entry;

            debug_printf("NMI %#hhx, %#hx, %#hhx\n", nmi->acpi_processor_uid, nmi->flags, nmi->local_apic_lint);
        }
        // Select next entry
        entry = (void*)((uintptr_t)entry + entry->length);
    }

    framebuffer_print("Done scanning table\n");

    framebuffer_printf("PIT maps to interrupt line %d\n", pit_entry_number);
    io_apic_interrupt_redirection(pit_entry_number, 33);

    // TODO: This shouldn't be hardcoded but I wanted to test the interrupt api
    io_apic_interrupt_redirection(1, 34);

    // Now knowing which interrupt the pit maps to, we can use it to calibrate a more precise timer
    timer_init();

    framebuffer_print("Timer setup complete\n");
}


void smp_init(void) {
    // Send initialization interupts to each AP
    for (int i = 0; i < cpu_count; i++) {
        // But not to the one we're already running on
        if (processor_local_data[i].core_id != this_cpu->core_id) {
            // Start up the core

        }
    }
}
