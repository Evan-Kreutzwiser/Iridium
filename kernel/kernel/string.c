
#include <kernel/string.h>
#include "arch/debug.h"

#include <stddef.h>


size_t strlen(const char *str) {
    if (str == NULL) {
        debug_print("Strlen of null!\n");
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
