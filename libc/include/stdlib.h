
#ifndef _LIBC_STDLIB_H_
#define _LIBC_STDLIB_H_

#ifdef __cplusplus
extern "C" {
#endif

#define __need_size_t
#include <stddef.h>

#ifndef NULL
#define NULL ((void*)0)
#endif

void *malloc(size_t n);
void *realloc(void *ptr, size_t n);
void *calloc(size_t c, size_t n);
void free(void *ptr);

int atoi(const char *str);
long atol(const char *str);

int abs(int);
long labs(long);

void exit(int status);

#ifdef __cplusplus
}
#endif

#endif // _LIBC_STDLIB_H_
