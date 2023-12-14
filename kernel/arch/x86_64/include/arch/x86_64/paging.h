
#ifndef ARCH_X86_64_PAGING_H_
#define ARCH_X86_64_PAGING_H_

#include <stdint.h>
#include <stddef.h>
#include <iridium/types.h>
#include <types.h>

typedef uint64_t page_table_entry;

// Setup the physical map and kernel mappings
void paging_init(void);

ir_status_t paging_map_page(page_table_entry *table, v_addr_t virtual_address, p_addr_t physical_address, uint64_t protection_flags);
ir_status_t paging_protect_page(page_table_entry *table, v_addr_t virtual_address, uint64_t protection_flags);
ir_status_t paging_unmap_page(page_table_entry *table, v_addr_t virtual_address);

// Split an entry in a table into a full, lower level table mapping the same memory
ir_status_t paging_split_page(page_table_entry *table_entry, uint table_level);

page_table_entry *paging_allocate_table();

#endif // ARCH_X86_64_PAGING_H_