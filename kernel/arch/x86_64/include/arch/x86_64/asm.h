#ifndef ARCH_X86_64_ASM_H_
#define ARCH_X86_64_ASM_H_

static inline char in_port_b(int port) {
    char input;
    asm volatile ("inb %%dx, %%al" : "=a"(input) : "d"(port));
    return input;
}

static inline char in_port_w(int port) {
    int input;
    asm volatile ("inb %%dx, %%ax" : "=a"(input) : "d"(port));
    return input;
}

static inline char in_port_l(int port) {
    int input;
    asm volatile ("inb %%dx, %%eax" : "=a"(input) : "d"(port));
    return input;
}

static inline void out_port_b(int port, char value) {
    asm volatile ("outb %%al, %%dx" : : "d"(port), "a"(value));
}

static inline void out_port_w(int port, int value) {
    asm volatile ("outb %%ax, %%dx" : : "d"(port), "a"(value));
}

static inline void out_port_l(int port, int value) {
    asm volatile ("outb %%eax, %%dx" : : "d"(port), "a"(value));
}

static inline void hlt() {
    asm volatile ("hlt");
}

#endif // ARCH_X86_64_ASM_H_
