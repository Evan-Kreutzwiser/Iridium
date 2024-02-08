
#ifndef _LIBC_IOPORT_H_
#define _LIBC_IOPORT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <iridium/types.h>

// Wrappers for handle management system calls

ir_status_t ir_handle_duplicate(ir_handle_t handle, ir_rights_t new_rights, ir_handle_t *new_handle_out);

ir_status_t ir_handle_replace(ir_handle_t handle, ir_rights_t new_rights, ir_handle_t *new_handle_out);

ir_status_t ir_handle_close(ir_handle_t id);

void ir_handle_dump(void);


#ifdef __cplusplus
}
#endif

#endif // _LIBC_IOPORT_H_
