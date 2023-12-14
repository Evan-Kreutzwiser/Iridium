# Syscalls

This file is an incomplete list outlining the various system calls made available by the Iridium kernel.

The list uses vDSO function names (Which is yet to be implemented), of functions that expose raw system calls. For internal names see `syscalls.c` and `iridium/syscalls.h`.

### Warning

This document is incomplete, and the API is subject to change.

## Handles

- `ir_handle_duplicate`
    - Create a copy of an existing handle, optionally changing the access rights. useful for transfering handles to another process through IPC.
- `ir_handle_replace`
    - Create a new handle to an object, invalidating the old one. This call can be used to reduce handle permissions
- `ir_handle_close`
    - Remove the handle. Once all handles to a kernel resource are discarded, the object is garbage collected and its memory can be reused.


## Processes / Threads

System calls related to creating and manipulating processes. See `process.md` for more details.

- `ir_process_create`
    - Creates a new process object and returns a handle to it. The program should then use other syscalls to set it up and create the initial thread.
- `ir_thread_create`
    - Create a new thread to execute under a process.
- `ir_thread_start`
    - Start execution of a new process created with `ir_thread_create`
- `ir_process_exit`
    - End the process with an exit code.

## Virtual Memory Objects

## Virtual Address Regions

## Interrupts

## Debugging

Miscalaneous calls created primarily for debugging purposed.

- `ir_yield`
    - Give up the remaining time slice to another process, as a rudimentary form of sleeping
- `ir_serial_out`
    - Provides limited printf functionality that prints to the kernel's serial output line
- `ir_debug_get_framebuffer`
    - Returns a readable/writeable `vm_object` that can be mapped into a processes address space, giving it direct access to the platform-provided framebuffer (No graphics driver exist yet).
- `ir_debug_dump_handles`
    - Object/handle debugging call that prints out information about the caller's owned handles.
