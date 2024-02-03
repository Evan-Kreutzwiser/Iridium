
#include "iridium/errors.h"
#include "kernel/arch/arch.h"
#include "kernel/channel.h"
#include "kernel/handle.h"
#include "kernel/heap.h"
#include "kernel/process.h"
#include "kernel/spinlock.h"
#include "kernel/string.h"

/// @brief Dynamically sized container for channel messages.
struct channel_message {
    size_t message_length;
    size_t handle_count;

    /// @brief Block of data containing `hanle_count` handle pointers
    /// followed by `message_length` bytes of data.
    char data[];
};

/// @brief Create a linked pair of channels for IPC
/// One of the 2 generated channels is intended to be passed to another process.
/// @param channel_out
/// @param peer_out
/// @return `IR_OK` on success or `IR_ERROR_NO_MEMORY`
ir_status_t channel_create(struct channel **channel_out, struct channel **peer_out) {
    struct channel *channel = calloc(1, sizeof(struct channel));
    struct channel *peer = calloc(1, sizeof(struct channel));

    if (!channel || !peer) {
        // Freeing null is a no-op
        free(channel);
        free(peer);
        return IR_ERROR_NO_MEMORY;
    }

    channel->peer = peer;
    peer->peer = channel;
    channel->object.type = OBJECT_TYPE_CHANNEL;
    peer->object.type = OBJECT_TYPE_CHANNEL;

    *channel_out = channel;
    *peer_out = peer;
    return IR_OK;
}

/// @brief Write a message to a channel
/// @param destination The channel the sent message is to be read from
/// @param message Data sent as part of the message
/// @param message_length Length of the message in bytes
/// @param handles Handles to the objects being shared/transfered. The
///                handles' IDs will ignored and reassigned upon reading
///                the message
/// @param handles_count Number of handle pointers in `handles`
/// @return
ir_status_t channel_write(struct channel *destination, char *message, size_t message_length, struct handle **handles, size_t handles_count) {

    struct channel_message *item = calloc(1, sizeof(size_t) * 2 + sizeof(uintptr_t) * handles_count + message_length);

    item->message_length = message_length;
    item->handle_count = handles_count;
    memcpy(&item->data, handles, handles_count * sizeof(uintptr_t));
    memcpy((void*)((uintptr_t)&item->data + sizeof(uintptr_t) * handles_count), message, message_length);

    linked_list_add(&destination->message_queue, item);

    return IR_OK;
}

/// @brief Channel garbagee collection
/// The channel's pair will still be able to read queued data,
/// but attempts to send data or read past the end of the queue will return `IR_ERROR_PEER_CLOSED`
void channel_cleanup(struct channel *channel) {
    if (channel->peer) {
        spinlock_aquire(channel->peer->object.lock);

        object_set_signals(&channel->peer->object, channel->peer->object.signals | CHANNEL_SIGNAL_PEER_DISCONNECTED);
        channel->peer = NULL;

        spinlock_release(channel->peer->object.lock);
    }

    struct channel_message *message;
    while (linked_list_remove(&channel->message_queue, 0, (void**)&message) == IR_OK) {
        // Start of data block is an array of handle pointers
        struct handle **handles = (void*)&message->data;
        for (uint i = 0; i < message->handle_count; i++) {
            object_decrement_references(handles[i]->object);
            free(handles[i]);
        }

        free(message);
    }

    free(channel);
}

/// @brief SYSCALL_CHANNEL_CREATE
/// Creates a pair of connected channel objects. One is intended to be handed to another process.
ir_status_t sys_channel_create(ir_handle_t *channel_out, ir_handle_t *peer_out) {
    if (!arch_validate_user_pointer(channel_out) || !arch_validate_user_pointer(peer_out)) {
        return IR_ERROR_INVALID_ARGUMENTS;
    }

    struct channel *channel, *peer;
    ir_status_t status = channel_create(&channel, &peer);
    if (status != IR_OK) return status;

    struct process *process = (struct process*)this_cpu->current_thread->object.parent;

    struct handle *channel_handle, *peer_handle;
    handle_create(process, &channel->object, IR_RIGHT_ALL, &channel_handle);
    handle_create(process, &peer->object, IR_RIGHT_ALL, &peer_handle);

    linked_list_add(&process->handle_table, channel_handle);
    linked_list_add(&process->handle_table, peer_handle);

    *channel_out = channel_handle->handle_id;
    *peer_out = peer_handle->handle_id;

    return IR_OK;
}

/// @brief SYSCALL_CHANNEL_READ
/// NOTE: I'm not satisfied with how this uses a single buffer for both handles and the message,
///       but I ran out of argument registers and this method saves validating another pointer.
/// @param channel A handle to the channel being read from
/// @param buffer A buffer large enough to contain the handle IDs and message content about to be read
/// @param buffer_length Size of `buffer` in bytes
/// @param handles_count Output parameter containing the number of handles in the read message
/// @param message_length Output parameter
/// @return `IR_OK on success`, or an error code. `handles_count` and `message_length` are still written
///         in the event of an `IR_ERROR_BUFFER_TOO_SMALL` error, indicating how large the caller should
///         make `buffer` before trying again.
ir_status_t sys_channel_read(ir_handle_t channel, char *buffer, size_t buffer_length, ir_handle_t *handles_count, size_t *message_length) {
    if (!arch_validate_user_pointer(buffer) || !arch_validate_user_pointer(handles_count) || !arch_validate_user_pointer(message_length)) {
        return IR_ERROR_INVALID_ARGUMENTS;
    }

    struct process *process = (struct process*)this_cpu->current_thread->object.parent;
    spinlock_aquire(process->handle_table_lock);

    struct handle *channel_handle;
    ir_status_t status = linked_list_find(&process->handle_table, (void*)channel, handle_by_id, NULL, (void**)&channel_handle);
    if (status != IR_OK) {
        spinlock_release(process->handle_table_lock);
        return IR_ERROR_BAD_HANDLE;
    }
    if (channel_handle->object->type != OBJECT_TYPE_CHANNEL) {
        spinlock_release(process->handle_table_lock);
        return IR_ERROR_WRONG_TYPE;
    }

    struct channel *channel_object = (struct channel*)channel_handle->object;
    spinlock_aquire(channel_handle->object->lock);

    struct channel_message *message;
    if (linked_list_get(&channel_object->message_queue, 0, (void**)&message) != IR_OK) {
        spinlock_release(channel_handle->object->lock);
        spinlock_release(process->handle_table_lock);
        return IR_ERROR_NOT_FOUND;
    }

    if (message->message_length + message->handle_count * sizeof(ir_handle_t) > buffer_length) {
        spinlock_release(channel_handle->object->lock);
        spinlock_release(process->handle_table_lock);
        return IR_ERROR_BUFFER_TOO_SMALL;
    }

    linked_list_remove(&channel_object->message_queue, 0, NULL);

    // Copy just the byte data portion of the message
    // The handles are stored as kernel pointers and must be transfered first
    memcpy(&buffer[sizeof(ir_handle_t) * message->handle_count], &message->data[sizeof(ir_handle_t) * message->handle_count], message->message_length);

    // Handles being transfered to the process need new IDs valid in this context
    for (uint i = 0; i < message->handle_count; i++) {
        // TODO: Does this work as intended
        struct handle *handle = *(struct handle**)&message->data[sizeof(uintptr_t) * i];
        handle->handle_id = handle_get_next_id(process);
        linked_list_add(&process->handle_table, handle);
        ((ir_handle_t*)buffer)[i] = handle->handle_id;
    }

    return IR_OK;
}

/// @brief SYSCALL_CHANNEL_WRITE
/// Handles must have `IR_RIGHT_TRANSFER` and are removed from the process's handle table.
ir_status_t sys_channel_write(ir_handle_t channel, char *message, size_t message_length, ir_handle_t **handles, size_t handles_count) {
    if (!arch_validate_user_pointer(message) || !arch_validate_user_pointer(handles)) {
        return IR_ERROR_INVALID_ARGUMENTS;
    }

    struct process *process = (struct process*)this_cpu->current_thread->object.parent;
    spinlock_aquire(process->handle_table_lock);

    struct handle *channel_handle;
    ir_status_t status = linked_list_find(&process->handle_table, (void*)channel, handle_by_id, NULL, (void**)&channel_handle);
    if (status != IR_OK) {
        spinlock_release(process->handle_table_lock);
        return IR_ERROR_BAD_HANDLE;
    }
    if (channel_handle->object->type != OBJECT_TYPE_CHANNEL) {
        spinlock_release(process->handle_table_lock);
        return IR_ERROR_WRONG_TYPE;
    }

    spinlock_aquire(channel_handle->object->lock);

    // `channel_write` will copy the pointers internally.
    // The already allocated handles will be assigned new ids in the destination process
    struct handle **handle_pointers = malloc(handles_count * sizeof(uintptr_t));

    // Verify all the handles exist and can be transfered
    for (uint i = 0; i < handles_count; i++) {
        ir_status_t status = linked_list_find(&process->handle_table, handles[i], handle_by_id, NULL, (void**)&handle_pointers[i]);
        if (status != IR_OK) {
            spinlock_release(channel_handle->object->lock);
            spinlock_release(process->handle_table_lock);
            free(handle_pointers);
            return status;
        }
        if (~handle_pointers[i]->rights & IR_RIGHT_TRANSFER) {
            spinlock_release(channel_handle->object->lock);
            spinlock_release(process->handle_table_lock);
            free(handle_pointers);
            return IR_ERROR_ACCESS_DENIED;
        }
    }

    // Remove the handles from the caller
    // The handles will continue to keep their referred objects alive.
    for (uint i = 0; i < handles_count; i++) {
        linked_list_find_and_remove(&process->handle_table, handle_pointers[i], NULL, NULL);
        linked_list_add(&process->free_handle_ids, (void*)handle_pointers[i]->handle_id);
    }

    channel_write((struct channel*)channel_handle->object, message, message_length, handle_pointers, handles_count);

    spinlock_release(channel_handle->object->lock);
    spinlock_release(process->handle_table_lock);
    free(handle_pointers);

    return IR_OK;
}
