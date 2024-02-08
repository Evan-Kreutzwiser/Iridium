
#include "kernel/object.h"

#include "iridium/errors.h"
#include "kernel/arch/arch.h"
#include "kernel/channel.h"
#include "kernel/handle.h"
#include "kernel/heap.h"
#include "kernel/interrupt.h"
#include "kernel/ioport.h"
#include "kernel/main.h"
#include "kernel/memory/v_addr_region.h"
#include "kernel/memory/vm_object.h"
#include "kernel/process.h"
#include "kernel/scheduler.h"
#include "kernel/time.h"

#include "arch/debug.h"

/// @brief Functions for operating on a type of object
/// TODO: Only cleanup function is actually used.
///       Possibly add a generic object info getter?
struct obj_functions {
    object_cleanup cleanup;
};

struct obj_functions functions[] = {
    [OBJECT_TYPE_V_ADDR_REGION] = {
        .cleanup = (object_cleanup)(uintptr_t)v_addr_region_cleanup
    },
    [OBJECT_TYPE_VM_OBJECT] = {
        .cleanup = (object_cleanup)(uintptr_t)vm_object_cleanup
    },
    [OBJECT_TYPE_PROCESS] = {
        .cleanup = (object_cleanup)(uintptr_t)process_cleanup
    },
    [OBJECT_TYPE_THREAD] = {
        .cleanup = (object_cleanup)(uintptr_t)thread_cleanup
    },
    [OBJECT_TYPE_CHANNEL] = {
        .cleanup = (object_cleanup)(uintptr_t)channel_cleanup
    },
    [OBJECT_TYPE_INTERRUPT] = {
        .cleanup = (object_cleanup)(uintptr_t)interrupt_cleanup
    },
    [OBJECT_TYPE_IOPORT] = {
        .cleanup = (object_cleanup)(uintptr_t)ioport_cleanup
    }
};

/// @brief Reduce the reference counter of an object, and maybe release it
///
/// @warning Must have a lock on the object before calling
/// @param obj An object which now has one less referrer
void object_decrement_references(object* obj) {
    if (obj->type == 0) {
        debug_printf("Attempted to decrease references of invalid object %#p\n", obj);
        panic(NULL, -1, "Attempted to decrease references of type 0 object (invalid)");
    }

    obj->references--;

    if (obj->references > 10000) {
        debug_printf("Something isn't right here. Type %d object has too many references (underflow?)\n", obj->type);
    }

    if (obj->references == 0) {
        debug_printf("Releasing object of type %d\n", obj->type);
        spinlock_aquire(obj->lock);
        //debug_printf("Releasing unreferenced object of type %d @ %#p\n", obj->type, obj);
        // The object is no longer being used anywhere

        // struct signal_listener *listener;
        // while(linked_list_remove(&obj->event_listeners, 0, &listener) == IR_OK) {
        //     if (listener->asynchronous) {
        //         // Delete outstanding messages
        //         void *packet;
        //         while(linked_list_remove(&listener->message_queue, 0, &packet)) {
        //             free(packet);
        //         }
        //     }

        //     free(listener);
        // }

        // Use the function corresponding to the object type to perform cleanup
        functions[obj->type].cleanup(obj);
    }
}

/// @brief Update an object's signals and trigger connected listeners if applicable
/// This should be used instead of directly setting the value of object->signals
/// @note Call with a lock on `obj`
void object_set_signals(object *obj, ir_signal_t signals) {
    obj->signals = signals;

    // TODO: Linked list iterator
    //
    uint i = 0;
    while (i < obj->signal_listeners.count) {
        struct signal_listener *listener;
        ir_status_t status = linked_list_get(&obj->signal_listeners, i, (void**)&listener);
        if (status != IR_OK) { debug_printf("Failed to get item %d from listeners\n", i); while (1); }
        if (listener->target_signals & signals) {
            linked_list_remove(&obj->signal_listeners, i, NULL);
            listener->observed_signals = signals;
            scheduler_unblock_listener(listener);
            i--;
        }
        i++;
    }

}

/// @brief Blocking syscall that waits until an object asserts a signal
/// @param object_handle Object whose signals will be listened to
/// @param target_signals Bitmap of signals to wait for
/// @param timeout_microseconds When to stop waiting.
///                             0 causes the current state to return immediately, and -1
///                             means never time out.
/// @param observed_signals Upon signal assertion, is set to contain the object's signals
/// @return On signal assertion, returns `IR_OK` and the complete state of the object is
///         found in observed signals. If deadline is reached first, returns `IR_ERROR_TIMED_OUT`
ir_status_t sys_object_wait(ir_handle_t object_handle, ir_signal_t target_signals, size_t timeout_microseconds, ir_signal_t *observed_signals) {
    if (!arch_validate_user_pointer(observed_signals)) {
        return IR_ERROR_INVALID_ARGUMENTS;
    }

    struct process *process = (struct process*)this_cpu->current_thread->object.parent;
    spinlock_aquire(process->handle_table_lock);

    struct handle *handle;
    ir_status_t status = linked_list_find(&process->handle_table, (void*)object_handle, handle_by_id, NULL, (void**)&handle);
    if (status != IR_OK) {
        spinlock_release(process->handle_table_lock);
        return IR_ERROR_BAD_HANDLE;
    }
    struct object *object = handle->object;

    spinlock_aquire(object->lock);
    spinlock_release(process->handle_table_lock);

    // If one of the watched signals is already asserted
    if (object->signals & target_signals) {
        *observed_signals = target_signals;
        spinlock_release(object->lock);
        return IR_OK;
    }

    if (timeout_microseconds > 0) {
        struct signal_listener* listener = malloc(sizeof(struct signal_listener));
        if (!listener) {
            spinlock_release(object->lock);
            return IR_ERROR_NO_MEMORY;
        }

        listener->thread = this_cpu->current_thread;
        listener->target = object;
        listener->target_signals = target_signals;
        listener->deadline = microseconds_since_boot + timeout_microseconds;

        // TODO: Find better way to prevent overflows
        // -1 means never expire
        if (timeout_microseconds == -1ul) {
            listener->deadline = -1;
        }

        // Keep the object alive, even in the even of another
        // thread freeing the handle used to make the listener
        object->references++;
        linked_list_add(&object->signal_listeners, listener);
        spinlock_release(object->lock);

        // Block the task until one of the signals are asserted
        scheduler_block_listener_and_switch(listener);

        // This is not reached until either the signal is raised or the deadline is reached.
        ir_signal_t signals = listener->observed_signals;
        *observed_signals = signals;

        free(listener);

        object_decrement_references(object);

        // If any of the target signals were raised
        if (signals & target_signals) {
            return IR_OK;
        }
    }
    else {
        *observed_signals = object->signals;

        // If any of the target signals were raised
        if (object->signals & target_signals) {
            return IR_OK;
        }
    }

    return IR_ERROR_TIMED_OUT;
}
