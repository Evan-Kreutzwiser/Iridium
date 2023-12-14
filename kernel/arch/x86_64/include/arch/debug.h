
#ifndef DEBUG_H
#define DEBUG_H

void debug_init(void);

// Print a single character to the serial output
void debug_print_char(char c);

// Print a string to the serial console
void debug_print(char* string);

void debug_printf(const char *restrict format, ...);

#ifdef DEBUG
#define debug_assert(cond, message) if (cond) debug_print(message)
#else
#define debug_assert(cond, message)
#endif

#endif // ! DEBUG_H
