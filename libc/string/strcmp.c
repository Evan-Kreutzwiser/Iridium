#include <string.h>

int strcmp(const char *str1, const char *str2) {
    const unsigned char *string1 = (const unsigned char *)str1;
    const unsigned char *string2 = (const unsigned char *)str2;

    while (*string1 == *string2 && *string1 != '\0') {
        string1++;
        string2++;
    }

    return *string1 - *string2;
}
