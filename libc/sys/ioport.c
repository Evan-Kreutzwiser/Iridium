#include <sys/ioport.h>
#include <sys/x86_64/syscall.h>
#include <iridium/types.h>

#define __need_size_t
#include <stddef.h>

ir_status_t ir_ioport_create(unsigned long vector, size_t count, ir_handle_t *out) {
    return _syscall_3(SYSCALL_IOPORT_CREATE, vector, count, (long)out);
}

ir_status_t ir_ioport_send(ir_handle_t ioport, size_t offset, long value, long word_size) {
    return _syscall_4(SYSCALL_IOPORT_SEND, ioport, offset, value, word_size);
}

ir_status_t ir_ioport_receive(ir_handle_t ioport, size_t offset, long word_size, long *out) {
    return _syscall_4(SYSCALL_IOPORT_RECEIVE, ioport, offset, word_size, (long)out);
}
