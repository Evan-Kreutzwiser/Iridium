
#include <kernel/stack.h>
#include <stdint.h>

// A temporary stack used during early booting
__attribute__((aligned(BOOT_STACK_ALIGN)))
uint8_t boot_stack[BOOT_STACK_SIZE];

// Stack used for exceptions in the tss
__attribute__((aligned(BOOT_STACK_ALIGN)))
uint8_t stack[BOOT_STACK_SIZE];
