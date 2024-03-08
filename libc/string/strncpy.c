#include <string.h>

char *strncpy(char *dest, const char *src, size_t n) {
    char *str = dest;

    while (n && *src != '\0') {
        *str++ = *src++;
        n--;
    }

    for (; n > 0; n--) {
        *str++ = '\0';
    }

    return dest;
}
