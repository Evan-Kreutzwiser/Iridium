
#include <sys/interrupt.h>
#include <sys/x86_64/syscall.h>
#include <iridium/types.h>

// TODO: Will be changed in the future to allocate vectors automacitally based on interrupt priority
ir_status_t ir_interrupt_create(long vector, long irq, ir_handle_t *out) {
    return _syscall_3(SYSCALL_INTERRUPT_CREATE, vector, irq, (long)out);
}

ir_status_t ir_interrupt_wait(ir_handle_t interrupt_handle) {
    return _syscall_1(SYSCALL_INTERRUPT_WAIT, interrupt_handle);
}

ir_status_t ir_interrupt_arm(ir_handle_t interrupt_handle) {
    return _syscall_1(SYSCALL_INTERRUPT_ARM, interrupt_handle);
}
