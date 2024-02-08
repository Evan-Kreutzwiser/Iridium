#include <stddef.h>
#include <sys/channel.h>
#include <sys/x86_64/syscall.h>
#include <iridium/syscalls.h>
#include <iridium/types.h>

ir_handle_t _fs_channel;

void _set_fs_channel(ir_handle_t new_channel) {
    _fs_channel = new_channel;
}

ir_status_t ir_channel_create(ir_handle_t *channel_out, ir_handle_t *peer_out) {
    return _syscall_2(SYSCALL_CHANNEL_CREATE, (long)channel_out, (long)peer_out);
}

ir_status_t ir_channel_read(ir_handle_t channel, char *buffer, size_t buffer_length, size_t *handles_count, size_t *message_length) {
    return _syscall_5(SYSCALL_CHANNEL_READ, channel, (long)buffer, buffer_length, (long)handles_count, (long)message_length);
}

ir_status_t ir_channel_write(ir_handle_t channel, char *message, size_t message_length, ir_handle_t **handles, size_t handles_count) {
    return _syscall_5(SYSCALL_CHANNEL_WRITE, channel, (long)message, message_length, (long)handles, (long)handles_count);
}
