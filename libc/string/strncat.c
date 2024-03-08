#include <string.h>

char *strncat(char *dest, const char *src, size_t n) {
    char *str = dest + strlen(dest);

    while (n && *src != '\0') {
        *str++ = *src++;
        n--;
    }

    *str = '\0';

    return dest;
}
