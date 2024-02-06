
#ifndef _LIBC_STDLIB_H_
#define _LIBC_STDLIB_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NULL
#define NULL ((void*)0)
#endif

int atoi(const char *str);

long atol(const char *str);

int abs(int);

long labs(long);

#ifdef __cplusplus
}
#endif

#endif // _LIBC_STDLIB_H_
