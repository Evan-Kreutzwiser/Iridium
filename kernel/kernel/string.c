
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

    while (*string1 == *string2) {
        if (* string1 == 0) { // When both strings end at the same time
            return 0;
        }

        string1++;
        string2++;
    }

    if (*string1 < *string2) {
        return -1;
    }
    else {
        return 1;
    }
}

int strncmp(const char *str1, const char *str2, size_t n) {
    while (*str1 == *str2 && n && *str1 != '\0' && *str2 != '\0') {
        str1++;
        str2++;
        n--;
    }
    if (n == 0) {
        return n;
    }
    else if (*str1 < *str2) {
        return -1;
    }
    else {
        return 1;
    }
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
