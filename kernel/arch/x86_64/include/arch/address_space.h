
#ifndef ARCH_X86_64_ADDRESS_SPACE_H_
#define ARCH_X86_64_ADDRESS_SPACE_H_

#include "arch/x86_64/paging.h"
#include "kernel/spinlock.h"

/// @brief Address space data structure
typedef struct address_space {

    page_table_entry *table_base;
    lock_t lock;

} address_space;

#endif // ARCH_X86_64_ADDRESS_SPACE_H_
