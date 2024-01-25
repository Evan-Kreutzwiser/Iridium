#ifndef KERNEL_TIME_H_
#define KERNEL_TIME_H_

#include "iridium/types.h"
#include <stddef.h>

extern size_t volatile microseconds_since_boot;

ir_status_t sys_time_microseconds(size_t *out);

#endif // KERNEL_TIME_H_
