
#include "sys/x86_64/syscall.h"
#include <stdnoreturn.h>

#include <stddef.h>
#include <stdlib.h>
#include <limits.h>

typedef void (*exit_function)(void);

exit_function* atexit_functions = NULL;
int exit_functions_count = 0;

int atexit(void (*function)(void)) {
    if (exit_functions_count == INT_MAX || function == NULL) return -1;

    atexit_functions = realloc(atexit_functions, sizeof(exit_function*) * ++exit_functions_count);
    atexit_functions[exit_functions_count - 1] = function;
    return 0;
}

noreturn void exit(int exit_code) {
    // atexit functions are called in the reverse order of which they were added
    for (int i = exit_functions_count - 1; i >= 0; i--) {
        atexit_functions[i]();
    }
    _Exit(exit_code);
}

noreturn void _Exit(int exit_code) {
    _syscall_1(SYSCALL_PROCESS_EXIT, (long)exit_code);
    while (1);
}
