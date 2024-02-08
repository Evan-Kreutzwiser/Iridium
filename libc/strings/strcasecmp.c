#include <ctype.h>
#include <strings.h>

int strcasecmp(const char *s1, const char *s2) {
    const unsigned char *string1 = (const unsigned char *)s1;
    const unsigned char *string2 = (const unsigned char *)s2;

    while (tolower(*string1) == tolower(*string2) && *string1 != '\0') {
        string1++;
        string2++;
    }

    return tolower(*string1) - tolower(*string2);
}

int strncasecmp(const char *s1, const char *s2, size_t n) {
    const unsigned char *string1 = (const unsigned char *)s1;
    const unsigned char *string2 = (const unsigned char *)s2;

    while (n && (tolower(*string1) == tolower(*string2)) && *string1 != '\0') {
        string1++;
        string2++;
        n--;
    }

    return tolower(*string1) - tolower(*string2);
}
