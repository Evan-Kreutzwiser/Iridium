/// @brief A heap implementation for allocating kernel objects

#ifndef KERNEL_HEAP_H_
#define KERNEL_HEAP_H_

#define __need_size_t
#include "stddef.h"

// Basic malloc/free for the kernel heap
void *__attribute__((malloc)) malloc(size_t n);

// Allocated memory is zerod
void *calloc(size_t num_items, size_t item_size);

// Copy memory to a new block of memory with a different size
void *realloc(void *pointer, size_t new_size);

void  free(void *data);

#endif // ! KERNEL_HEAP_H_
