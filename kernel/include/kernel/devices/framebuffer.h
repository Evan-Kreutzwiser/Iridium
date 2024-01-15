#ifndef KERNEL_DEVICES_FRAMEBUFFER_H_
#define KERNEL_DEVICES_FRAMEBUFFER_H_

#include "iridium/types.h"
#include "types.h"

void init_framebuffer(p_addr_t location, int width, int height, int pitch, int bits_per_pixel);

void framebuffer_fill_screen(unsigned char r, unsigned char g, unsigned char b);

void framebuffer_print(const char *string);

// Use a global buffer to print to the framebuffer (Not thread safe)
void framebuffer_printf(const char * restrict format, ...);

void framebuffer_set_cursor_pos(int x, int y);

/// @brief SYSCALL_DEBUG_GET_FRAMEBUFFER
ir_status_t sys_framebuffer_get(ir_handle_t *framebuffer, int *width, int *height, int *pitch, int *bits_per_pixel);

#endif // KERNEL_DEVICES_FRAMEBUFFER_H_
