/// @file include/types.h
/// @brief Common primitive data types used in the kernel

#ifndef TYPES_H_
#define TYPES_H_

#include <stdint.h>

// Used by the memory manager to communicate which addresses are physical or virtual

/// @brief An address in virtual memory. Usually an address in kernel space.
typedef uintptr_t v_addr_t;
/// @brief An address in physical memory.
/// Access these addresses indirectly through the physical map.
typedef uintptr_t p_addr_t;

typedef unsigned int uint;

#endif // TYPES_H_
