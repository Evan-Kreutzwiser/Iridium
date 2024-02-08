#ifndef KERNEL_CHANNEL_H_
#define KERNEL_CHANNEL_H_

#include "kernel/object.h"
#include "kernel/linked_list.h"

/// @see kernel/handle.h
struct handle;
/// @see kernel/process.h
struct process;
/// Internal message storage
struct channel_message;

/// @brief IPC object for transmitting data and object handles between processes.
///
/// NOTE: Channels do not keep their peers alive. If one end closes communication will fail
struct channel {
    object object;

    /// This channel's other end where data is sent
    struct channel *peer;
    linked_list message_queue;
};

/// @brief Create a linked pair of channels for IPC
/// One of the 2 generated channels is intended to be passed to another process =
/// @param channel_out
/// @param peer_out
/// @return `IR_OK` on success or `IR_ERROR_NO_MEMORY`
ir_status_t channel_create(struct channel **channel_out, struct channel **peer_out);

/// @brief Read the next message in a channel's queue
/// @param channel
/// @param process
/// @param message_out Buffer for the data component of the message
/// @param handles Array of handles
/// @return `IR_OK` on success
ir_status_t channel_read(struct channel *channel, struct process *process, char *message_out, struct handle *handles);

/// @brief Write a message to a channel
/// @param destination The channel the sent message is to be read from
/// @param message Data sent as part of the message
/// @param message_length Length of the message in bytes
/// @param handles Handles to the objects being shared/transfered. The
///                handles' IDs will ignored and reassigned upon reading
///                the message
/// @param handles_count Number of handle pointers in `handles`
/// @return
ir_status_t channel_write(struct channel *destination, char *message, size_t message_length, struct handle **handles, size_t handles_count);

/// Channel garbage collection handler
void channel_cleanup(struct channel *channel);

/// @brief SYSCALL_CHANNEL_CREATE
/// Create a linked pair of channel objects for IPC, facilitating the transfer of
/// byte data and object handles. One is intended to be transferred to another process.
ir_status_t sys_channel_create(ir_handle_t *channel_out, ir_handle_t *peer_out);

/// @brief SYSCALL_CHANNEL_READ
/// @param channel A handle to the channel being read from
/// @param buffer A buffer large enough to contain the handle IDs and message content about to be read
/// @param buffer_length Size of `buffer` in bytes
/// @param handles_count Output parameter containing the number of handles in the read message
/// @param message_length Output parameter
/// @return `IR_OK on success`, or an error code. `handles_count` and `message_length` are still written
///         in the event of an `IR_ERROR_BUFFER_TOO_SMALL` error, indicating how large the caller should
///         make `buffer` before trying again.
ir_status_t sys_channel_read(ir_handle_t channel, char *buffer, size_t buffer_length, size_t *handles_count, size_t *message_length);

/// @brief SYSCALL_CHANNEL_WRITE
/// Handles must have `IR_RIGHT_TRANSFER` and are removed from the process's handle table.
ir_status_t sys_channel_write(ir_handle_t channel, char *message, size_t message_length, ir_handle_t **handles, size_t handles_count);

#endif // KERNEL_CHANNEL_H_
