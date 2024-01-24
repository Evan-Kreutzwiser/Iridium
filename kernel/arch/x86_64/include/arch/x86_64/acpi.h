#ifndef ARCH_X86_64_ACPI_H_
#define ARCH_X86_64_ACPI_H_

#include "arch/debug.h"

#include "types.h"
#include <stdint.h>
#include <stdbool.h>

#define ACPI_RSDP_SIGNATURE "RSD PTR "

struct acpi_rsdp_v2 {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;
    // V2 fields start here
    // Check that revision is 2 before accessing
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t extended_checksum;
    uint8_t reserved[3];
} __attribute__((packed));

// Standard header for acpi tables
struct acpi_header {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    uint64_t oem_table_id;
    uint32_t oem_revision;
    uint32_t creator_id; // Does this have to be char[]?
    uint32_t creator_revision;
} __attribute__((packed));

struct rsdt {
    struct acpi_header header;
    uint32_t sdt_pointers[];
} __attribute__((packed));

struct xsdt {
    struct acpi_header header;
    uint64_t sdt_pointers[];
} __attribute__((packed));

struct address_structure
{
    uint8_t address_space_id;    // 0 - system memory, 1 - system I/O
    uint8_t register_bit_width;
    uint8_t register_bit_offset;
    uint8_t reserved;
    uint64_t address;
} __attribute__((packed));

struct acpi_fadt {
    struct acpi_header header;
    uint32_t firmware_ctrl;
    uint32_t dsdt;

    // field used in acpi 1.0; no longer in use, for compatibility only
    uint8_t reserved;

    uint8_t preferred_power_management_profile;
    uint16_t sci_interrupt;
    uint32_t smi_command_port;
    uint8_t acpi_enable;
    uint8_t acpi_disable;
    uint8_t s4bios_req;
    uint8_t pstate_control;
    uint32_t pm1a_event_block;
    uint32_t pm1b_event_block;
    uint32_t pm1a_control_block;
    uint32_t pm1b_control_block;
    uint32_t pm2_control_block;
    uint32_t pm_timer_block;
    uint32_t gpe0_block;
    uint32_t gpe1_block;
    uint8_t pm1_event_length;
    uint8_t pm1_control_length;
    uint8_t pm2_control_length;
    uint8_t pm_timer_length;
    uint8_t gpe0_length;
    uint8_t gpe1_length;
    uint8_t gpe1_base;
    uint8_t c_state_control;
    uint16_t worst_c2_latency;
    uint16_t worst_c3_latency;
    uint16_t flush_size;
    uint16_t flush_stride;
    uint8_t duty_offset;
    uint8_t duty_width;
    uint8_t day_alarm;
    uint8_t month_alarm;
    uint8_t century;

    // reserved in acpi 1.0; used since acpi 2.0+
    uint16_t boot_architecture_flags;

    uint8_t reserved2;
    uint32_t flags;

    struct address_structure reset_reg;

    uint8_t reset_value;
    uint8_t reserved3[3];

    // 64 bit pointers - available on acpi 2.0+
    uint64_t x_firmware_control;
    uint64_t x_dsdt;

    struct address_structure x_pm1a_event_block;
    struct address_structure x_pm1b_event_block;
    struct address_structure x_pm1a_control_block;
    struct address_structure x_pm1b_control_block;
    struct address_structure x_pm2_control_block;
    struct address_structure x_pm_timer_block;
    struct address_structure x_gpe0_block;
    struct address_structure x_gpe1_block;
} __attribute__((packed));

struct acpi_hpet {
    struct acpi_header header;
    uint32_t event_timer_block_id;
    struct address_structure base_address;
    uint8_t hpet_number; // Systems might have multiple HPETs?
    uint16_t minimum_clock_tick; // Main counter minimum clock tick in periodic mode
    uint8_t page_protection_attribute; // Systems might have multiple HPETs?
} __attribute__((packed));

// This struct is followed by entries describing APICs
struct acpi_madt {
    struct acpi_header header;
    uint32_t local_apic_address;
    uint32_t flags;
} __attribute__((packed));

#define ACPI_MADT_ENTRY_PROCESSOR_LOCAL_APIC 0
#define ACPI_MADT_ENTRY_IO_APIC 1
#define ACPI_MADT_ENTRY_INTTERUPT_SOURCE_OVERRIDE 2

#define ACPI_MADT_ENTRY_LOCAL_APIC_NMI 4
// There are other entry types not listed here

struct madt_entry_header {
    uint8_t type;
    uint8_t length;
} __attribute__((packed));

struct processor_local_apic {
    struct madt_entry_header header;
    uint8_t acpi_processor_id; // UID for the processor object cooresponding to this core
    uint8_t apic_id; // ID of the core's local apic
    uint32_t flags;
} __attribute__((packed));

struct io_apic {
    struct madt_entry_header header;
    uint8_t id; // ID of the IOAPIC
    uint8_t reserved;
    uint32_t address; // IO APIC physical address
    uint32_t global_interrupt_base; // Global system interrupt number where this IOAPIC's interrupts start
} __attribute__((packed));

struct interrupt_source_override {
    struct madt_entry_header header;
    uint8_t bus_source;
    uint8_t irq_source;
    uint32_t global_system_interrupt;
    uint16_t flags;
} __attribute__((packed));

struct local_apic_nmi {
    struct madt_entry_header header;
    uint8_t acpi_processor_uid;
    uint16_t flags;
    uint8_t local_apic_lint;
} __attribute__((packed));


static inline bool acpi_checksum(const struct acpi_header *header) {
    unsigned char sum = 0;
    char *data = (char*)header;
    for (unsigned int i = 0; i < header->length; i++) {
        sum += data[i];
    }
    return sum == 0;
}

void apic_init();
void apic_send_eoi();

struct registers;
void timer_fired(struct registers* context);

void acpi_init(v_addr_t rsdp_addr);
int get_cpu_count();
void smp_init();

#endif // ! ARCH_X86_64_ACPI_H_
