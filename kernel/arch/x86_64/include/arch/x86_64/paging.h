
#ifndef ARCH_X86_64_PAGING_H_
#define ARCH_X86_64_PAGING_H_

#include <stdint.h>
#include <stddef.h>
#include <iridium/types.h>
#include <types.h>
#include <stdbool.h>

typedef uint64_t page_table_entry;

extern bool no_execute_supported;

struct physical_region; // Defined in kernel/memory/pmm.h

// Setup the physical map and kernel mappings
void paging_init(struct physical_region *memory_regions, size_t count);

ir_status_t paging_map_page(page_table_entry *table, v_addr_t virtual_address, p_addr_t physical_address, uint64_t protection_flags, bool paging_map_page);
ir_status_t paging_protect_page(page_table_entry *table, v_addr_t virtual_address, uint64_t protection_flags);
ir_status_t paging_unmap_page(page_table_entry *table, v_addr_t virtual_address);

void paging_print_tables(p_addr_t table_root, v_addr_t target);

#endif // ARCH_X86_64_PAGING_H_
