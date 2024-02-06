
#include "sys/x86_64/syscall.h"
#include <stdnoreturn.h>

noreturn void exit(int exit_code) {
    _syscall_1(SYSCALL_PROCESS_EXIT, (long)exit_code);
    while (1);
}
