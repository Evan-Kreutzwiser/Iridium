#ifndef KERNEL_DEVICES_FRAMEBUFFER_H_
#define KERNEL_DEVICES_FRAMEBUFFER_H_

#include "iridium/types.h"
#include "types.h"

void init_framebuffer(p_addr_t location, int width, int height, int pitch, int bits_per_pixel);

/// @brief SYSCALL_DEBUG_GET_FRAMEBUFFER
ir_status_t sys_framebuffer_get(ir_handle_t *framebuffer, int *width, int *height, int *pitch, int *bits_per_pixel);

#endif // KERNEL_DEVICES_FRAMEBUFFER_H_
