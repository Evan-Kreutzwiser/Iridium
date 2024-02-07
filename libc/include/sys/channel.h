
#ifndef _LIBC_CHANNEL_H_
#define _LIBC_CHANNEL_H_

#ifdef __cplusplus
extern "C" {
#endif

#define __need_size_t
#include <stddef.h>
#include <iridium/types.h>

// Iridium channel used for communicating with the filesystem server.
// Typically provided as an argument by libc to newly spawned processes.
extern ir_handle_t _fs_channel;

void _set_fs_channel(ir_handle_t new_channel);

// Wrappers for raw channel system calls

ir_status_t ir_channel_create(ir_handle_t *channel_out, ir_handle_t *peer_out);

ir_status_t ir_channel_read(ir_handle_t channel, char *buffer, size_t buffer_length, size_t *handles_count, size_t *message_length);

ir_status_t ir_channel_write(ir_handle_t channel, char *message, size_t message_length, ir_handle_t **handles, size_t handles_count);

#ifdef __cplusplus
}
#endif

#endif // _LIBC_CHANNEL_H_
