#include <string.h>

char *strcpy(char *dest, const char *src) {
    char *str = dest;

    do {
        *str++ = *src++;
    }
    while (*src != '\0');

    return dest;
}
