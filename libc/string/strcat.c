#include <string.h>

char *strcat(char *dest, const char *src) {
    char *str = dest + strlen(dest);

    do {
        *str++ = *src++;
    }
    while (*src != '\0');

    return dest;
}
