#include <string.h>

char *strchr(const char *str, int c) {
    unsigned char *string = (unsigned char*)str;
    while (*string != '\0') {
        if (*string == (unsigned char)c) {
            return (char *)string;
        }
        string++;
    }

    // The null terminator is considered part of the string in the context of this function
    return (unsigned char)c == '\0' ? (char *)string : NULL;
}
