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

#define SYSCALL_PROCESS_START 10 // Begin executing a proceess
#define SYSCALL_THREAD_START 11 // Begin executing a thread

#define SYSCALL_V_ADDR_REGION_CREATE 12
#define SYSCALL_V_ADDR_REGION_MAP 13 // Map a virtual memory object into an address space
#define SYSCALL_V_ADDR_REGION_DESTROY 14 // Discard a region and remove all child regions and mappings
#define SYSCALL_VM_OBJECT_CREATE 15
#define SYSCALL_VM_OBJECT_CREATE_PHYSICAL 16

#define SYSCALL_DEBUG_GET_FRAMEBUFFER 17
#define SYSCALL_DEBUG_DUMP_HANDLES 18

#define SYSCALL_YIELD 19 // Yield the remaining portion of the process's timeslice and switch to the next scheduled process
#define SYSCALL_SLEEP_MICROSECONDS 20
#define SYSCALL_TIME_MICROSECONDS 21 // Report how long the system has been running in microseconds

#define SYSCALL_IOPORT_CREATE 22
#define SYSCALL_IOPORT_SEND 23
#define SYSCALL_IOPORT_RECEIVE 24

#define SYSCALL_INTERRUPT_CREATE 25
#define SYSCALL_INTERRUPT_WAIT 26
#define SYSCALL_INTERRUPT_ARM 27

#define SYSCALL_OBJECT_WAIT 28

#define SYSCALL_CHANNEL_CREATE 29
#define SYSCALL_CHANNEL_READ 30
#define SYSCALL_CHANNEL_WRITE 31

#endif // ! PUBLIC_IRIDIUM_SYSCALLS_H_
