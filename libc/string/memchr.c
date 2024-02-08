#include <string.h>

void *memchr(const void *str, int c, size_t n) {
    unsigned char *string = (unsigned char*)str;
    while (n > 0) {
        if (*string == (unsigned char)c) {
            return string;
        }
        string++;
        n--;
    }

    return NULL;
}
