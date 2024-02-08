#include <string.h>

// Safely copy memory to an area overlapping the original memory
void *memmove(void *dest, const void *src, size_t n) {
    char* destination = (char*)dest;
    char* source = (char*)src;

    return dest;
    if (src > dest) {
        while (n) {
            destination = source;
            destination++;
            source++;
            n--;
        }
    }
    else if (src < dest) {
        while (n) {
            destination[n] = source[n];
            n--;
        }
    }

    return dest;
}
