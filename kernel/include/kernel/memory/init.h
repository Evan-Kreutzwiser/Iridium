
#ifndef KERNEL_MEMORY_INIT_H_
#define KERNEL_MEMORY_INIT_H_

void physical_memory_init(void);
void virtual_memory_init(void);

/// Initialize the heap during system boot
//void heap_early_init();
/// Transition to a dynamically allocated heap
//void heap_finish_init();

/// Get the pages backing kernel code/data.
///
/// Only used for setting up the kernel `vm_object`s,
/// and contains pages for the bootstrap heap
struct physical_page_info *get_kernel_pages(void);

#endif // KERNEL_MEMORY_INIT_H_
