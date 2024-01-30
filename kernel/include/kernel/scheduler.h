
#ifndef KERNEL_SCHEDULER_H_
#define KERNEL_SCHEDULER_H_

#include "kernel/process.h"
#include "kernel/object.h"
#include "iridium/types.h"
#include <stdbool.h>
#include <stddef.h>

struct registers;

ir_status_t sys_yield();
ir_status_t sys_sleep_microseconds(size_t microseconds);

void schedule_thread(struct thread *thread);
void switch_task(bool reschedule);

ir_status_t scheduler_block_listener_and_switch(struct signal_listener *listener);
void scheduler_unblock_listener(struct signal_listener *listener);
void scheduler_sleep_microseconds(struct thread *thread, size_t microseconds);

#endif // KERNEL_SCHEDULER_H_
