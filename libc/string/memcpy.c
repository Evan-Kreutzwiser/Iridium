#include <string.h>

void *memcpy(void* dest, const void* src, size_t size) {
    char* destination = (char*)dest;
    char* source = (char*)src;

    for (size_t n = 0; n < size; n++) {
        destination[n] = source[n];
    }

    return dest;
}
