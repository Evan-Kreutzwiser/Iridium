
#ifndef _LIBC_IOPORT_H_
#define _LIBC_IOPORT_H_

#ifdef __cplusplus
extern "C" {
#endif

#define __need_size_t
#include <stddef.h>

#include <iridium/types.h>

// Wrappers for ioport system calls

ir_status_t ir_ioport_create(unsigned long vector, size_t count, ir_handle_t *out);

ir_status_t ir_ioport_send(ir_handle_t ioport, size_t offset, long value, long word_size);

ir_status_t ir_ioport_receive(ir_handle_t ioport, size_t offset, long word_size, long *out);


#ifdef __cplusplus
}
#endif

#endif // _LIBC_IOPORT_H_
