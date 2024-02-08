#include <string.h>

void *memset(void *str, int c, size_t n) {
    unsigned char *string = (unsigned char*)str;
    while (n > 0) {
        *string = (unsigned char)c;
        string++;
        n--;
    }

    return str;
}
