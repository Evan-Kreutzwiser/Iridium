
#include <kernel/string.h>
#include "arch/debug.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


size_t strlen(const char *str) {
    if (str == NULL) {
        return 0;
    }

    int length = 0;
    while (*str != '\0') {
        length++;
        str++;
    }
    return length;
}

int strcmp(const char *str1, const char *str2) {
    const unsigned char *string1 = (const unsigned char *)str1;
    const unsigned char *string2 = (const unsigned char *)str2;

    while (*string1 == *string2 && *string1 != '\0') {
        string1++;
        string2++;
    }

    return *string1 - *string2;
}

int strncmp(const char *str1, const char *str2, size_t n) {
    const unsigned char *s1 = (const unsigned char *)str1;
    const unsigned char *s2 = (const unsigned char *)str2;

    while (n && (*s1 == *s2) && *s1 != '\0') {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) {
        return 0;
    }

    return *s1 - *s2;
}

// Copy a section of memory from one location to another
void *memcpy(void* dest, const void* src, size_t size) {
    // TODO: This needs some serious optimization

    char* destination = (char*)dest;
    char* source = (char*)src;

    for (size_t n = 0; n < size; n++) {
        destination[n] = source[n];
    }

    return dest;
}

// Fill an area of memory with a specific value
void *memset(void *ptr, int value, size_t n) {
    unsigned char c = (unsigned char)value;
    char *str = ptr;
    while (n > 0) {
        *str = c;
        str++;
        n--;
    }

    return ptr;
}

int memcmp(const void *str1, const void *str2, size_t n) {
    const unsigned char *string1 = (const unsigned char *)str1;
    const unsigned char *string2 = (const unsigned char *)str2;

    while (*string1 == *string2 && n) {
        string1++;
        string2++;
        n--;
    }
    if (n == 0) {
        return n;
    }
    return *string1 - *string2;
}




#define ASCII_IS_NUMERIC(c) (c <= '9' && c >= '0')
#define ASCII_NUM_TO_INT(c) (c - '0')

static char hex_characters[] = "0123456789abcdef";
static char hex_characters_capital[] = "0123456789ABCDEF";

static long va_arg_from_width_signed(va_list va, int bits) {
    switch (bits) {
        case 8:
            return va_arg(va, int); // Promoted to int
        case 16:
            return va_arg(va, int); // Promoted to int
        case 32:
            return va_arg(va, int);
        case 64:
            return va_arg(va, long);
        default:
            return va_arg(va, int);
    }
}

static unsigned long va_arg_from_width_unsigned(va_list va, int bits) {
    switch (bits) {
        case 8:
            return va_arg(va, int); // Promoted to int
        case 16:
            return va_arg(va, int); // Promoted to int
        case 32:
            return va_arg(va, unsigned int);
        case 64:
            return va_arg(va, unsigned long);
        default:
            return va_arg(va, unsigned int);
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

/// @return Number of characters printed
static int print_decimal_signed(char *dest, long value, int min_length_zeros, bool display_plus, bool space_if_positive) {
    int characters_printed = 0;

    if (value < 0) {
        *dest = '-';
        characters_printed++;
        value *= -1;
    }
    else {
        // TODO: Is this the correct behavior?
        if (display_plus) {
            *dest = '+';
            characters_printed++;
        } else if (space_if_positive) {
            *dest = ' ';
            characters_printed++;
        }
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

    // Add leading zeros if applicable
    if (min_length_zeros - number_length > 0) {
        memset(&dest[characters_printed], '0', min_length_zeros - number_length);
        characters_printed += min_length_zeros - number_length;
    }
    memcpy(&dest[characters_printed], characters, number_length);
    characters_printed += number_length;

    return characters_printed;
}

/// @return Number of characters printed
static int print_decimal_unsigned(char *dest, uint64_t value, int min_length_zeros, bool display_plus, bool space_if_positive) {
    int characters_printed = 0;

    // TODO: Is this the correct behavior?
    if (display_plus) {
        *dest = '+';
        characters_printed++;
    } else if (space_if_positive) {
        *dest = ' ';
        characters_printed++;
    }

    // Don't print nothing when value is zero
    if (min_length_zeros == 0) min_length_zeros = 1;
    int number_length = value_length(value, 10);

    char characters[number_length];
    for (int i = number_length - 1; i >= 0; i--) {
        characters[i] = '0' + (value % 10);
        value /= 10;
    }

    // Add leading zeros if applicable
    if (min_length_zeros - number_length > 0) {
        memset(&dest[characters_printed], '0', min_length_zeros - number_length);
        characters_printed += min_length_zeros - number_length;
    }

    memcpy(&dest[characters_printed], characters, number_length);
    characters_printed += number_length;

    return characters_printed;
}

/// @return Number of characters printed
static int print_hex(char *dest, uint64_t value, bool print_0x, bool capitalized, int min_length_zeros) {

    char *character_set = hex_characters;
    if (capitalized) character_set = hex_characters_capital;

    int characters_printed = 0;

    if (print_0x) {
        memcpy(dest, "0x", 2);
        characters_printed += 2;
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
            dest[characters_printed] = (character_set[hex_value]);
            characters_printed++;
        }
    }

    return characters_printed;
}

void vsprintf(char *dest, const char *restrict format, va_list args) {

    while (*format != '\0') {

        if (*format == '%') {
            format++;

            // Double equal signs prints a single symbol
            if (*format == '%') {
                *dest = '%';
                dest++;
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
                        *dest = ' ';
                        dest++;
                    }

                    memcpy(dest, string, length);
                    dest += length;

                }
                else if (*format == 'd' || *format == 'i') {
                    long value = va_arg_from_width_signed(args, bits);

                    int length = max(value_length(value, 10), min_precision);
                    if (flag_plus || flag_space || value < 0) { length++; } // preceding symbol counts for the width

                    // Pad out length to specified width
                    char fill_character = flag_zero ? '0' : ' ';
                    for (int i = min_witdh; i < min_witdh; i++) {
                        *dest = fill_character;
                        dest++;
                    }

                    dest += print_decimal_signed(dest, value, min_precision, flag_plus, flag_space);
                }
                else if (*format == 'u') {
                    long value = va_arg_from_width_unsigned(args, bits);

                    int length = max(value_length(value, 10), min_precision);
                    if (flag_plus || flag_space) { length++; } // preceding symbol counts for the width

                    // Pad out length to specified width
                    char fill_character = flag_zero ? '0' : ' ';
                    for (int i = length; i < min_witdh; i++) {
                        *dest = fill_character;
                        dest++;
                    }

                    dest += print_decimal_unsigned(dest, value, min_precision, flag_plus, flag_space);
                }
                // Ignores flag_zero, flag_plus, and flag_space
                else if (*format == 'x' || *format == 'X' || *format == 'p') {
                    if (*format == 'p') { bits = sizeof(size_t) * 8; }
                    unsigned long value = va_arg_from_width_unsigned(args, bits);

                    int length = max(value_length(value, 16), min_precision);
                    if (flag_hastag) { length += 2; } // preceding symbol counts for the width

                    // Pad out length to specified width
                    for (int i = length; i < min_witdh; i++) {
                        *dest = ' ';
                        dest++;
                    }

                    dest += print_hex(dest, value, flag_hastag, *format == 'X', min_precision);
                }
                else if (*format == 'c') {
                    unsigned char value = va_arg(args, int); // Char promoted to int
                    for (int i = 1; i < min_witdh; i++) {
                        *dest = ' ';
                        dest++;
                    }
                    *dest = value;
                    dest++;
                }
                else {
                    // If there isn't a valid specifier the format++; Prevent blindly discarding
                    // the character, which may be a terminator we don't want to miss
                    format--;
                }
            }
        }
        else {
            *dest = *format;
            dest++;
        }

        format++;
    }

    *dest = '\0';
}

void sprintf(char *dest, const char * restrict format, ...) {
    va_list args;
    va_start(args, format);
    vsprintf(dest, format, args);
    va_end(args);
}
