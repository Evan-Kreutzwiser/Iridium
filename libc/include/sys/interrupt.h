
#ifndef _LIBC_INTERRUPT_H_
#define _LIBC_INTERRUPT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <iridium/types.h>

// Wrappers for interrupt system calls

ir_status_t ir_interrupt_create(long vector, long irq, ir_handle_t *out);

ir_status_t ir_interrupt_wait(ir_handle_t interrupt_handle);

ir_status_t ir_interrupt_arm(ir_handle_t interrupt_handle);

#ifdef __cplusplus
}
#endif

#endif // _LIBC_INTERRUPT_H_
