/// @brief Barebones spinlock implementation
///
/// Very basic mutual exclusion primitive for kernel code

#ifndef KERNEL_SPINLOCK_H_
#define KERNEL_SPINLOCK_H_

#include "kernel/cpu_locals.h"
#include "arch/debug.h"
#include <stdatomic.h>
#define __need_null
#include <stddef.h>

struct thread;

/// @brief Spinlock mutex
typedef struct lock_t {
    atomic_flag lock;
    const char *function;
} lock_t;

#define spinlock_aquire(x) \
    if ((x).function) debug_printf("Tried getting lock in %s:%d, but it is currently held by %s\n", __FILE__, __LINE__, (x).function); \
    do { \
    while (atomic_flag_test_and_set_explicit(&(x).lock, memory_order_acquire)); \
    (x).function = __func__; \
    } while (0)

#define spinlock_release(x) do { \
    (x).function = NULL; \
    atomic_flag_clear_explicit(&(x).lock, memory_order_release); \
    } while (0)

#endif // ! KERNEL_SPINLOCK_H_
