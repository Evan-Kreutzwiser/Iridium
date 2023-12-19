#ifndef KERNEL_MAIN_H_
#define KERNEL_MAIN_H_

#include "types.h"

void kernel_startup();
void kernel_main(v_addr_t initrd_start_address);

#endif // ! KERNEL_MAIN_H_
