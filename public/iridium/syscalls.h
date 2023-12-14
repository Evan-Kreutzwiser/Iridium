/// @file public/iridium/syscalls.h
/// @brief Numeric codes for system call functions

#ifndef PUBLIC_IRIDIUM_SYSCALLS_H_
#define PUBLIC_IRIDIUM_SYSCALLS_H_

// System call numbers
#define SYSCALL_HANDLE_DUPLICATE 1 // Create a new copy of a handle with a subset of the original's rights
#define SYSCALL_HANDLE_REPLACE 2 // Similar to SYSCALL_HANDLE_DUPLICATE but invalidates the original handle
#define SYSCALL_HANDLE_CLOSE 3 // Remove an open handle
#define SYSCALL_SERIAL_OUT 4 // For testing only
#define SYSCALL_PROCESS_EXIT 5 // Terminate the currently running process
#define SYSCALL_THREAD_EXIT 6 // Terminate the currently running thread

#define SYSCALL_PROCESS_CREATE 7 // Create a blank new process
#define SYSCALL_THREAD_CREATE 8 // Create a new thread

#define SYSCALL_PROCESS_START 16 // Begin executing a proceess
#define SYSCALL_THREAD_START 17 // Begin executing a thread

#define SYSCALL_V_ADDR_REGION_CREATE 9
#define SYSCALL_V_ADDR_REGION_MAP 10 // Map a virtual memory object into an address space
#define SYSCALL_V_ADDR_REGION_DESTROY 11 // Discard a region and remove all child regions and mappings
#define SYSCALL_VM_OBJECT_CREATE 12

#define SYSCALL_DEBUG_GET_FRAMEBUFFER 13
#define SYSCALL_DEBUG_DUMP_HANDLES 14

#define SYSCALL_YIELD 15 // Yield the remaining portion of the process's timeslice and switch to the next scheduled process

#define SYSCALL_IOPORT_CREATE 18
#define SYSCALL_IOPORT_SEND 19
#define SYSCALL_IOPORT_RECEIVE 20

#define SYSCALL_INTERRUPT_CREATE 21
#define SYSCALL_INTERRUPT_WAIT 22
#define SYSCALL_INTERRUPT_ARM 23

char *syscall_names[] = {
    [SYSCALL_HANDLE_DUPLICATE] = "SYSCALL_HANDLE_DUPLICATE",
    [SYSCALL_HANDLE_REPLACE] = "SYSCALL_HANDLE_REPLACE",
    [SYSCALL_HANDLE_CLOSE] = "SYSCALL_HANDLE_CLOSE",
    [SYSCALL_SERIAL_OUT] = "SYSCALL_SERIAL_OUT",
    [SYSCALL_PROCESS_EXIT] = "SYSCALL_PROCESS_EXIT",
    [SYSCALL_THREAD_EXIT] = "SYSCALL_THREAD_EXIT",
    [SYSCALL_PROCESS_CREATE] = "SYSCALL_PROCESS_CREATE",
    [SYSCALL_THREAD_CREATE] = "SYSCALL_THREAD_CREATE",
    [SYSCALL_PROCESS_START] = "SYSCALL_PROCESS_START",
    [SYSCALL_V_ADDR_REGION_CREATE] = "SYSCALL_V_ADDR_REGION_CREATE",
    [SYSCALL_V_ADDR_REGION_MAP] = "SYSCALL_V_ADDR_REGION_MAP",
    [SYSCALL_V_ADDR_REGION_DESTROY] = "SYSCALL_V_ADDR_REGION_DESTROY",
    [SYSCALL_VM_OBJECT_CREATE] = "SYSCALL_VM_OBJECT_CREATE",
    [SYSCALL_DEBUG_GET_FRAMEBUFFER] = "SYSCALL_DEBUG_GET_FRAMEBUFFER",
    [SYSCALL_DEBUG_DUMP_HANDLES] = "SYSCALL_DEBUG_DUMP_HANDLES",
    [SYSCALL_YIELD] = "SYSCALL_YIELD",
    [SYSCALL_IOPORT_CREATE] = "SYSCALL_IOPORT_CREATE",
    [SYSCALL_IOPORT_SEND] = "SYSCALL_IOPORT_SEND",
    [SYSCALL_IOPORT_RECEIVE] = "SYSCALL_IOPORT_RECEIVE",
    [SYSCALL_INTERRUPT_CREATE] = "SYSCALL_INTERRUPT_CREATE",
    [SYSCALL_INTERRUPT_WAIT] = "SYSCALL_INTERRUPT_WAIT",
    [SYSCALL_INTERRUPT_ARM] = "SYSCALL_INTERRUPT_ARM"
};

#endif // ! PUBLIC_IRIDIUM_SYSCALLS_H_
