
#include <stdlib.h>
#include <sys/x86_64/syscall.h>
#include <iridium/syscalls.h>

// Stack smashing protecter
// Whenever code detects a buffer overrun or other kinds of
// stack smashing this is called to terminate the program
void __stack_chk_fail(void) {
    // TODO: printf an error message
    _syscall_1(SYSCALL_SERIAL_OUT, (long)"Stack smashing detected, exiting\n");
    _Exit(1); // Don't call atexit functions
}
