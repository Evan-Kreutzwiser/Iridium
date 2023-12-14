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
static char hex_characters_capital[] = "0123456789ABCDEF";

static void outb(uint16_t port, uint8_t data) {
    asm volatile ("outb %0, %1" : : "a"(data), "Nd"(port));
}

// Check if the serial chip is ready to transmit data
static int debug_is_transmit_empty() {
    uint8_t volatile data;
    asm volatile ("inb %%dx,%%al":"=a" (data) : "d" (SERIAL_BASE_PORT + 5));
    return data & 0x20;
    //return 1;
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

////////////////////////////////////////////////////////////
// None of the following is meant to be pretty.
// It is made to work.

#define ASCII_IS_NUMERIC(c) (c <= '9' && c >= '0')
#define ASCII_NUM_TO_INT(c) (c - '0')

static long va_arg_from_width(va_list va, int bits) {
    switch (bits) {
        case 8:
            return va_arg(va, int); // Promoted to int
        case 16:
            return va_arg(va, int); // Promoted to int
        case 32:
            return va_arg(va, uint32_t);
        case 64:
            return va_arg(va, uint64_t);
        default:
            return va_arg(va, uint32_t);
    }
}

static int value_length(uint64_t value, int base) {
    int length = 1; // Always at least 1, to display a zero
    value /= base;
    while (value != 0) {
        value /= base;
        length++;
    }

    return length;
}

static int max(int a, int b) {
    if (a > b) return a;
    return b;
}

static void print_decimal_signed(int64_t value, int min_length_zeros, bool display_plus, bool space_if_positive) {
    if (value < 0) {
        debug_print_char('-');
        value *= -1;
    }
    else {
        // TODO: Is this the correct behavior?
        if (display_plus) debug_print_char('+');
        else if (space_if_positive) debug_print_char(' ');
    }

    // Don't print nothing when value is zero
    if (min_length_zeros == 0) min_length_zeros = 1;
    int number_length = value_length(value, 10);

    char characters[number_length+1];
    characters[number_length] = '\0';
    for (int i = number_length - 1; i >= 0; i--) {
        characters[i] = '0' + (value % 10);
        value /= 10;
    }
    for (int i = min_length_zeros - number_length; i > 0; i--) {
        debug_print_char('0');
    }
    debug_print(characters);
}

static void print_decimal_unsigned(uint64_t value, int min_length_zeros, bool display_plus, bool space_if_positive) {
    // TODO: Is this the correct behavior?
    if (display_plus) debug_print_char('+');
    else if (space_if_positive) debug_print_char(' ');

    // Don't print nothing when value is zero
    if (min_length_zeros == 0) min_length_zeros = 1;
    int number_length = value_length(value, 10);

    char characters[number_length+1];
    characters[number_length] = '\0';
    for (int i = number_length - 1; i >= 0; i--) {
        characters[i] = '0' + (value % 10);
        value /= 10;
    }
    for (int i = min_length_zeros - number_length; i > 0; i--) {
        debug_print_char('0');
    }
    debug_print(characters);
}

static void print_hex(uint64_t value, bool print_0x, bool capitalized, int min_length_zeros) {

    char *character_set = hex_characters;
    if (capitalized) character_set = hex_characters_capital;

    if (print_0x) {
        debug_print("0x");
    }

    // Don't print nothing when value is zero
    if (min_length_zeros <= 0) min_length_zeros = 1;

    bool print_zeros = false;

    for (int digit = 15; digit >= 0; digit--) {
        int hex_value = (value >> digit * 4) & 0xf;

        if (digit + 1 <= min_length_zeros || hex_value != 0) {
            print_zeros = true;
        }

        if (print_zeros || hex_value != 0) {
            debug_print_char(character_set[hex_value]);
        }
    }
}

/// @brief Printf for the serial line for debugging purposes.
///
/// `%[flags][width][.precision][length]specifier` \n
/// Supports a subset of printf syntax. \n
/// `%%s` - print a string \n
/// `%%c` - print a string \n
/// `%%d` - print a signed decimal number \n
/// `%%x` or `%%X` - print a hexdecimal value, in lowercase or capitals respectively \n
///
/// `%%s` and `%%x`/`%%X` support specifying a minimum width to be met
/// by left-padding with spaces, and specifying precision pads
/// hexdecimal numbers with 0s to meet a minimum width.
///
/// The `#` flag can be used to add a leading 0x to hexdecimal numbers. For signed
/// decimal numbers, the `+` flag forces a plus to be shown for positive numbers,
/// and the ` ` flag displays a space if a sign would not be written.
void debug_printf(const char *restrict format, ...) {

    va_list args;
    va_start(args, format);

    while (*format != '\0') {

        if (*format == '%') {
            format++;

            // Double equal signs prints a single symbol
            if (*format == '%') {
                debug_print_char('%');
            }
            else {

                bool flag_hastag = false; // Insert a 0x for hex numbers
                bool flag_space = false; // If no sign is to be written, insert a blank space
                bool flag_plus = false;
                bool flag_zero = false; // Left-pads with 0 instead of spaces for witdh

                // Check flags
                bool adding_flags = true;
                while (adding_flags) {
                    switch (*format) {
                        case '#':
                            flag_hastag = true;
                            break;
                        case '+':
                            flag_plus = true;
                            break;
                        case ' ':
                            flag_space = true;
                            break;
                        case '0':
                            flag_zero = true;
                            break;
                        default:
                            adding_flags = false;
                            format--;
                    }
                    format++;
                }

                // Check text width
                int min_witdh = 0;
                while (ASCII_IS_NUMERIC(*format)) {
                    min_witdh *= 10;
                    min_witdh += ASCII_NUM_TO_INT(*format);
                    format++;
                }

                // Check number precision
                int min_precision = -1; // -1 indicates it is unset
                if (*format == '.') {
                    min_precision = 0;
                    format++;
                    while (ASCII_IS_NUMERIC(*format)) {
                        min_precision *= 10;
                        min_precision += ASCII_NUM_TO_INT(*format);
                        format++;
                    }
                }

                int bits = sizeof(int) * 8;

                // Check value length
                switch (*format) {
                    case 'h':
                        format++;
                        if (*format =='h') {
                            bits = sizeof(char) * 8;
                            format++;
                        }
                        else {
                            bits = sizeof(short) * 8;
                        }
                        break;
                    case 'l':
                        format++;
                        if (*format =='l') {
                            bits = sizeof(long long) * 8;
                            format++;
                        }
                        else {
                            bits = sizeof(long) * 8;
                        }
                        break;
                    case 'z':
                        format++;
                        bits = sizeof(size_t) * 8;
                        break;
                    case 'j':
                        format++;
                        bits = sizeof(intmax_t) * 8;
                        break;
                    case 't':
                        format++;
                        bits = sizeof(ptrdiff_t) * 8;
                        break;
                }

                // Specifiers
                if (*format == 's') { // Character string
                    char *string = va_arg(args, char*);
                    int length = strlen(string);

                    if (min_precision >= 0) {
                        length = min_precision;
                    }

                    // left-pad the string with 0s if it is too short
                    for (int i = length; i < min_witdh; i++) {
                        debug_print_char(' ');
                    }

                    if (min_precision < 0) {
                        debug_print(string);
                    }
                    else {
                        // If specified, limit string length to precision
                        while (length > 0 && *string != '\0') {
                            debug_print_char(*string);
                            length--;
                            string++;
                        }
                    }
                }
                else if (*format == 'd' || *format == 'i') {
                    long value = va_arg_from_width(args, bits);

                    int length = max(value_length(value, 10), min_precision);
                    if (flag_plus || flag_space || value < 0) { length++; } // preceding symbol counts for the width

                    // Pad out length to specified width
                    char fill_character = flag_zero ? '0' : ' ';
                    for (int i = min_witdh; i < min_witdh; i++) {
                        debug_print_char(fill_character);
                    }

                    print_decimal_signed(value, min_precision, flag_plus, flag_space);
                }
                else if (*format == 'u') {
                    long value = va_arg_from_width(args, bits);

                    int length = max(value_length(value, 10), min_precision);
                    if (flag_plus || flag_space) { length++; } // preceding symbol counts for the width

                    // Pad out length to specified width
                    char fill_character = flag_zero ? '0' : ' ';
                    for (int i = length; i < min_witdh; i++) {
                        debug_print_char(fill_character);
                    }

                    print_decimal_unsigned(value, min_precision, flag_plus, flag_space);
                }
                // Ignores flag_zero, flag_plus, and flag_space
                else if (*format == 'x' || *format == 'X' || *format == 'p') {
                    if (*format == 'p') { bits = sizeof(size_t) * 8; }
                    unsigned long value = va_arg_from_width(args, bits);

                    int length = max(value_length(value, 16), min_precision);
                    if (flag_hastag) { length += 2; } // preceding symbol counts for the width

                    // Pad out length to specified width
                    for (int i = length; i < min_witdh; i++) {
                        debug_print_char(' ');
                    }

                    print_hex(value, flag_hastag, *format == 'X', min_precision);
                }
                else if (*format == 'c') {
                    unsigned char value = va_arg(args, int); // Char promoted to int
                    for (int i = 1; i < min_witdh; i++) {
                        debug_print_char(' ');
                    }
                    debug_print_char(value);
                }
                else {
                    // If there isn't a valid specifier the format++; at the end blindly consumes
                    // the character, which may be a terminator we don't want to miss
                    format--;
                }
            }
        }
        else {
            debug_print_char(*format);
        }

        format++;
    }
}
