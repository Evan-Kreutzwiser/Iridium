#ifndef ARCH_X86_64_ACPI_H_
#define ARCH_X86_64_ACPI_H_

#include "arch/debug.h"

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


static inline bool acpi_checksum(struct acpi_header *header) {
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

void acpi_init();
int get_cpu_count();
void smp_init();

#endif // ! ARCH_X86_64_ACPI_H_
