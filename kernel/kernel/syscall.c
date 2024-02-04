/// @file kernel/syscall.c
/// @brief Generic side of responding to system calls
/// @see Architecture specfic syscall entry point

#include "iridium/errors.h"
#include "iridium/syscalls.h"
#include "kernel/channel.h"
#include "kernel/devices/framebuffer.h"
#include "kernel/handle.h"
#include "kernel/heap.h"
#include "kernel/interrupt.h"
#include "kernel/ioport.h"
#include "kernel/memory/v_addr_region.h"
#include "kernel/memory/vm_object.h"
#include "kernel/process.h"
#include "kernel/scheduler.h"
#include "kernel/time.h"
#include <stdint.h>

#include "arch/debug.h"

#if __LONG_WIDTH__ < __INTPTR_WIDTH__
#error Long is shorter than pointer width
#endif

typedef int64_t (*syscall)(long, long, long, long, long);

int64_t sys_serial_out(long string, long arg1, long arg2, long arg3, long arg4);

const syscall syscall_table[] = {
    [SYSCALL_HANDLE_DUPLICATE] = (syscall)(uintptr_t)sys_handle_duplicate,
    [SYSCALL_HANDLE_REPLACE] = (syscall)(uintptr_t)sys_handle_replace,
    [SYSCALL_HANDLE_CLOSE] = (syscall)(uintptr_t)sys_handle_close,
    [SYSCALL_SERIAL_OUT] = (syscall)(uintptr_t)sys_serial_out,
    [SYSCALL_PROCESS_EXIT] = (syscall)(uintptr_t)sys_process_exit,
    [SYSCALL_THREAD_EXIT] = (syscall)(uintptr_t)sys_thread_exit,
    [SYSCALL_PROCESS_CREATE] = (syscall)(uintptr_t)sys_process_create,
    [SYSCALL_THREAD_CREATE] = (syscall)(uintptr_t)sys_thread_create,
    [SYSCALL_V_ADDR_REGION_CREATE] = (syscall)(uintptr_t)sys_v_addr_region_create,
    [SYSCALL_V_ADDR_REGION_MAP] = (syscall)(uintptr_t)sys_v_addr_region_map,
    [SYSCALL_V_ADDR_REGION_DESTROY] = (syscall)(uintptr_t)sys_v_addr_region_destroy,
    [SYSCALL_VM_OBJECT_CREATE] = (syscall)(uintptr_t)sys_vm_object_create,
    [SYSCALL_VM_OBJECT_CREATE_PHYSICAL] = (syscall)(uintptr_t)sys_vm_object_create_physical,
    [SYSCALL_DEBUG_GET_FRAMEBUFFER] = (syscall)(uintptr_t)sys_framebuffer_get,
    [SYSCALL_DEBUG_DUMP_HANDLES] = (syscall)(uintptr_t)sys_handle_dump,
    [SYSCALL_THREAD_START] = (syscall)(uintptr_t)sys_thread_start,
    [SYSCALL_YIELD] = (syscall)(uintptr_t)sys_yield,
    [SYSCALL_SLEEP_MICROSECONDS] = (syscall)(uintptr_t)sys_sleep_microseconds,
    [SYSCALL_TIME_MICROSECONDS] = (syscall)(uintptr_t)sys_time_microseconds,
    [SYSCALL_IOPORT_CREATE] = (syscall)(uintptr_t)sys_ioport_create,
    [SYSCALL_IOPORT_SEND] = (syscall)(uintptr_t)sys_ioport_send,
    [SYSCALL_IOPORT_RECEIVE] = (syscall)(uintptr_t)sys_ioport_receive,
    [SYSCALL_INTERRUPT_CREATE] = (syscall)(uintptr_t)sys_interrupt_create,
    [SYSCALL_INTERRUPT_WAIT] = (syscall)(uintptr_t)sys_interrupt_wait,
    [SYSCALL_INTERRUPT_ARM] = (syscall)(uintptr_t)sys_interrupt_arm,
    [SYSCALL_OBJECT_WAIT] = (syscall)(uintptr_t)sys_object_wait,
    [SYSCALL_CHANNEL_CREATE] = (syscall)(uintptr_t)sys_channel_create,
    [SYSCALL_CHANNEL_READ] = (syscall)(uintptr_t)sys_channel_read,
    [SYSCALL_CHANNEL_WRITE] = (syscall)(uintptr_t)sys_channel_write,
};

uint syscall_count = sizeof(syscall_table) / sizeof(syscall);

/// @brief Perform a system call made by a user process
/// @param syscall_num The id of the system call the kernel is requested to perform
/// @return The value returned by the performed system call
int64_t syscall_handler(unsigned int syscall_num, long arg0, long arg1, long arg2, long arg3, long arg4) {
    if (syscall_num > syscall_count || syscall_table[syscall_num] == NULL) {
        return IR_ERROR_INVALID_ARGUMENTS;
    }
    // Avoid leaving the kernel in a bad state by delaying potential termination until the syscall is complete
    this_cpu->current_thread->in_syscall = true;

    ir_status_t result = syscall_table[syscall_num](arg0, arg1, arg2, arg3, arg4);

    // TODO: Check that the process isn't being killed
    this_cpu->current_thread->in_syscall = false;

    if (this_cpu->current_thread->state == TERMINATING) {
        //thread_finish_termination(this_cpu->current_thread);
        // We can't free kernel stack while we're using it, so switch away and the
        // thread can be cleaned up next time it would have been scheduled
        switch_task(true);
    }

    return result;
}

// Print a null terminated string over the serial debugging line
int64_t sys_serial_out(long string, long arg1, long arg2, long arg3, long arg4) {
    // This is a debugging call only, and does no error checking!
    debug_printf((char*)string, arg1, arg2, arg3, arg4);
    framebuffer_printf((char*)string, arg1, arg2, arg3, arg4);
    return IR_OK;
}
