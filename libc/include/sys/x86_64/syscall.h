

#ifndef _LIBC_SYSCALLS_H_
#define _LIBC_SYSCALLS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "iridium/syscalls.h"

// The syscall instruction uses rcx and r11 for context switching
// r10 is used in the place of rcx as arg2

static inline int _syscall_1(long syscall_num, long arg0) {
    int result;
    asm volatile ("syscall" : "=a" (result) : "D" (syscall_num), "S" (arg0) : "rcx", "r11", "memory");
    return result;
}

static inline int _syscall_2(long syscall_num, long arg0, long arg1) {
    int result;
    asm volatile ("syscall" : "=a" (result) : "D" (syscall_num), "S" (arg0), "d" (arg1) : "rcx", "r11", "memory");
    return result;
}

static inline int _syscall_3(long syscall_num, long arg0, long arg1, long arg2) {
    int result;
    register long r10 asm("r10") = arg2;
    asm volatile ("syscall" : "=a" (result) : "D" (syscall_num), "S" (arg0), "d" (arg1), "r" (r10) : "rcx", "r11", "memory");
    return result;
}

static inline int _syscall_4(long syscall_num, long arg0, long arg1, long arg2, long arg3) {
    int result;
    register long r10 asm("r10") = arg2;
    register long r8 asm("r8") = arg3;
    asm volatile ("syscall" : "=a" (result) : "D" (syscall_num), "S" (arg0), "d" (arg1), "r" (r10), "r" (r8) :  "rcx", "r11", "memory");
    return result;
}

static inline int _syscall_5(long syscall_num, long arg0, long arg1, long arg2, long arg3, long arg4) {
    int result;
    register long r10 asm("r10") = arg2;
    register long r8 asm("r8") = arg3;
    register long r9 asm("r9") = arg4;
    asm volatile ("syscall" : "=a" (result) : "D" (syscall_num), "S" (arg0), "d" (arg1), "r" (r10), "r" (r8), "r" (r9) : "rcx", "r11", "memory");
    return result;
}
#ifdef __cplusplus
}
#endif

#endif // _LIBC_SYSCALLS_H_
