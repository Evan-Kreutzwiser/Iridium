
#ifndef _LIBC_V_ADDR_REGION_H_
#define _LIBC_V_ADDR_REGION_H_

#ifdef __cplusplus
extern "C" {
#endif

#define __need_size_t
#include <stddef.h>
#include <iridium/types.h>

// Wrappers for raw vm object system calls

ir_status_t ir_v_addr_region_create(size_t size, size_t flags, ir_handle_t *handle_out);

ir_status_t ir_v_addr_region_map(ir_handle_t parent, ir_handle_t vm_object, size_t flags, ir_handle_t *region_out, void **address_out);

ir_status_t ir_v_addr_region_destroy(ir_handle_t region);

#ifdef __cplusplus
}
#endif

#endif // _LIBC_V_ADDR_REGION_H_
