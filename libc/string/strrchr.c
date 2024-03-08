#include <string.h>

// Find the last occurance of c in str. If c is '\0', returns a pointer to the end of the string.
char *strrchr(const char *str, int c) {
    char *last_location = NULL;
    do {
        if (*str == c)
            last_location = (char *)str;
    }
    while (*str++ != '\0');

    return last_location;
}
