
#ifndef KERNEL_IOPORT_H_
#define KERNEL_IOPORT_H_

#include "iridium/types.h"
#include "kernel/object.h"
#include "types.h"

#define __need_size_t
#include "stddef.h"

struct ioport {
    object object;

    uint base_port;
    size_t range_length;
};

ir_status_t ioport_create(uint vector, size_t count, struct ioport **out);
void ioport_cleanup(struct ioport *range);

ir_status_t sys_ioport_create(unsigned long vector, size_t count, ir_handle_t *out);

/// @brief Send a value to an io port
/// @param ioport IO port or port range
/// @param offset Index into port range
/// @param value Value to output
/// @param word_size Bit size of the value to send
/// @return
ir_status_t sys_ioport_send(ir_handle_t ioport, size_t offset, long value, long word_size);

/// @brief Read a value from an io port
/// @param ioport IO port or port range
/// @param offset Index into port range
/// @param word_size Bit size of the value to recieve
/// @param out Pointer to location where output should be written. Datatype must match `word_size`.
/// @return
ir_status_t sys_ioport_receive(ir_handle_t ioport, size_t offset, long word_size, long *out);

#endif // KERNEL_IOPORT_H_
