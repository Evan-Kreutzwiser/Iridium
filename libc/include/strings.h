
#ifndef _LIBC_STRINGS_H_
#define _LIBC_STRINGS_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NULL
#define NULL ((void*)0)
#endif

#define __need_size_t
#include <stddef.h>

#include <string.h>


#define index(str, c) strchr(str, c);
#define rindex(str, c) strrchr(str, c);

static inline void bzero(void *s, size_t n) {
    memset(s, 0, n);
}

static inline void bcopy(const void *src, void *dest, size_t n) {
    memmove(dest, src, n);
}

static inline int bcmp(const void *s1, const void *s2, size_t n) {
    return memcmp(s1, s2, n);
}

int strcasecmp(const char *s1, const char *s2);
int strncasecmp(const char *s1, const char *s2, size_t n);

static inline int ffs(int i) {
    if (i == 0) return 0;

    int b = 1;
    while ((i & 1) == 0) {
        b++;
        i >>= 1;
    }

    return b;
}

#ifdef __cplusplus
}
#endif

#endif // _LIBC_STRINGS_H_
