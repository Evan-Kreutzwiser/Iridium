#ifndef KERNEL_OBJECT_H_
#define KERNEL_OBJECT_H_

#include "types.h"
#include "kernel/linked_list.h"
#include "kernel/spinlock.h"

/// Freed by the wait handler
struct signal_listener {
    struct object *target; /// The object whose signals are being listened to
    struct thread *thread; /// Thread listening for signals
    ir_signal_t target_signals; // Bit mask of which signals should trigger the listener
    ir_signal_t observed_signals; // Bit map signals currently high when the signal is sent
    unsigned long deadline; // TODO: Time type
};

/// @brief Common component of all kernel objects
typedef struct object {
    uint type;
    _Atomic uint references;
    struct object *parent;
    linked_list children;
    ir_signal_t signals;
    linked_list signal_listeners; // Signal listeners attached to the object

    lock_t lock;
} object;

typedef void (*object_cleanup)(object* obj); // Frees an object

/// @brief Update an object's signals and trigger connected listeners if applicable
/// This should be used instead of directly setting the value of object->signals
void object_set_signals(object *obj, ir_signal_t signals);

/// @brief Blocking syscall that waits until an object asserts a signal
/// @param object_handle Object whose signals will be listened to
/// @param target_signals Bitmap of signals to wait for
/// @param deadline When to stop waiting
/// @param observed_signals Upon signal assertion, is set to contain the object's signals
/// @return On signal assertion, returns `IR_OK` and the complete state of the object is
///         found in observed signals. If deadline is reached first, returns `IR_ERROR_TIMED_OUT`
ir_status_t sys_object_wait(ir_handle_t object_handle, ir_signal_t target_signals, size_t timeout_microseconds, ir_signal_t *observed_signals);

void object_decrement_references(object* obj);

#endif // ! KERNEL_OBJECT_H_
