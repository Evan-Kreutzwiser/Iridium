
#ifndef KERNEL_SCHEDULER_H_
#define KERNEL_SCHEDULER_H_

#include "kernel/process.h"
#include "kernel/object.h"
#include "iridium/types.h"
#include <stdbool.h>

struct registers;

ir_status_t sys_yield();
void schedule_thread(struct thread *thread);
void switch_task(bool reschedule);

void scheduler_block_listener_and_switch(struct signal_listener *listener);
void scheduler_unblock_listener(struct signal_listener *listener);

#endif // KERNEL_SCHEDULER_H_
