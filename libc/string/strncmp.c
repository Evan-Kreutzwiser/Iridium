#include <string.h>

int strncmp(const char *str1, const char *str2, size_t n) {
    const unsigned char *s1 = (const unsigned char *)str1;
    const unsigned char *s2 = (const unsigned char *)str2;

    while (n && (*s1 == *s2) && *s1 != '\0') {
        s1++;
        s2++;
        n--;
    }

    return *s1 - *s2;
}
