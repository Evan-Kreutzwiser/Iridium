/// @file arch/x86_64/debug.c
/// @brief Printing debugging information over the serial port

#include "arch/debug.h"
#include "kernel/string.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>


#define SERIAL_BASE_PORT 0x3f8

static char hex_characters[] = "0123456789abcdef";

static inline void outb(uint16_t port, uint8_t data) {
    asm volatile ("outb %0, %1" : : "a"(data), "d"(port));
}

// Check if the serial chip is ready to transmit data
static inline int debug_is_transmit_empty() {
    uint8_t volatile data;
    asm volatile ("inb %1, %%al":"=a" (data) : "d" (SERIAL_BASE_PORT + 5));
    return data & 0x20;
}

// Set up the serial port
void debug_init() {
    outb(SERIAL_BASE_PORT + 1, 0x0); // Disable interrupts
    outb(SERIAL_BASE_PORT + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    outb(SERIAL_BASE_PORT + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
    outb(SERIAL_BASE_PORT + 1, 0x00);    //                  (hi byte)
    outb(SERIAL_BASE_PORT + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(SERIAL_BASE_PORT + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    outb(SERIAL_BASE_PORT + 4, 0x0B);    // IRQs enabled, RTS/DSR set
}

void debug_print_char(char c) {
    while (debug_is_transmit_empty() == 0);
    outb(SERIAL_BASE_PORT, c);
}

/// @brief Print a null terminated string over the serial line
void debug_print(char* string) {
    for (int character = 0; string[character] != '\0'; character++) {
        debug_print_char(string[character]);
    }
}

/// @brief Print a 64 bit hexdecimal value to the serial line
///
/// Prints a hexdecimal number with a leading 0x and all
/// leading 0s to a width of 16 digits
/// @param value The value to output
void debug_print_hex(uint64_t value) {
    debug_print("0x");
    for (int character = 15; character >= 0; character--) {
        debug_print_char(hex_characters[(value >> (character * 4)) & 0xf]);
    }
}

static char buffer[1024];

/// @brief Printf for the serial line for debugging purposes.
///
/// Supports a large but incomplete subset of the standard printf specifiers and behavior
void debug_printf(const char * restrict format, ...) {
    va_list args;
    va_start(args, format);
    vsprintf(buffer, format, args);
    va_end(args);

    // TODO: Does not support multithreading
    debug_print(buffer);
}
