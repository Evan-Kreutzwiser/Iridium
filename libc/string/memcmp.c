#include <string.h>

int memcmp(const void *str1, const void *str2, size_t n) {
    const unsigned char *string1 = (const unsigned char *)str1;
    const unsigned char *string2 = (const unsigned char *)str2;

    while (*string1 == *string2 && n) {
        string1++;
        string2++;
        n--;
    }
    return *string1 - *string2;
}
