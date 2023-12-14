
#include "kernel/memory/physical_map.h"
#include "stddef.h"

uintptr_t physical_map_base = 0xffff800000000000ul; // The physical map will be at -128 TB, the bottom of kernel space
size_t physical_map_length =        0x8000000000ul; // 512 GB
