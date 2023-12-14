
#define __need_size_t
#include <stddef.h>

/// @brief Get the length of a null terminated string
/// @param str A null terminated string
/// @return The length of the string, excluding the null terminator
size_t strlen(const char *str);
int strcmp(const char *str1, const char *str2);
int strncmp(const char *str1, const char *str2, size_t n);
/// Copy a section of memory from one location to another
void *memcpy(void* dest, const void* src, size_t size);
/// Fill a region of memory with a specific value
void *memset(void *ptr, int value, size_t n);
/// Compare blocks of memory
/// @return 0 when the blocks of memory are the same
int memcmp(const void *str1, const void *str2, size_t n);
