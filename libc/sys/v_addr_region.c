#include <sys/v_addr_region.h>
#include <sys/x86_64/syscall.h>
#include <iridium/syscalls.h>
#include <iridium/types.h>

ir_status_t ir_v_addr_region_create(size_t size, unsigned long flags, ir_handle_t *handle_out) {
    return _syscall_3(SYSCALL_V_ADDR_REGION_CREATE, size , flags, (long)handle_out);
}

ir_status_t ir_v_addr_region_map(ir_handle_t parent, ir_handle_t vm_object, size_t flags, ir_handle_t *region_out, void **address_out) {
    return _syscall_5(SYSCALL_V_ADDR_REGION_MAP, parent, vm_object, flags, (long)region_out, (long)address_out);
}

ir_status_t ir_v_addr_region_destroy(ir_handle_t region) {
    return _syscall_1(SYSCALL_V_ADDR_REGION_DESTROY, region);
}
