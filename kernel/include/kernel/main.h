#ifndef KERNEL_MAIN_H_
#define KERNEL_MAIN_H_

#include "kernel/memory/vm_object.h"
#include "types.h"
#include <stddef.h>

void kernel_startup();
void kernel_main(vm_object *initrd, v_addr_t initrd_start_address);

#endif // ! KERNEL_MAIN_H_
