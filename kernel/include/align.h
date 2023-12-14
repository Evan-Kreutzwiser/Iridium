/// @file include/align.h
/// @brief Address alignment helpers

#ifndef ALIGN_H_
#define ALIGN_H_

#include "arch/defines.h"

#define PAGE_ALIGNED __attribute__((aligned(PAGE_SIZE)))

// TODO: There has to be a better way to do this, but this works for now
#define ROUND_UP(x, multiple) ((uint64_t)(x) + (multiple-1) - (((uint64_t)(x)-1) % (multiple)))
#define ROUND_DOWN(x, multiple) ((uint64_t)(x) - (uint64_t)((uint64_t)(x) % (uint64_t)(multiple)))

#define ROUND_UP_PAGE(x) ROUND_UP(x, PAGE_SIZE)
// Optimized knowing page size will always be a power of 2
#define ROUND_DOWN_PAGE(x) ((x) & ~(PAGE_SIZE-1))

#endif // ALIGN_H_
