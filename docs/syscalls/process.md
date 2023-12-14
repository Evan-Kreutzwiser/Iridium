# Processes

System calls related to creating and manipulating processes.

### Warning

This document is incomplete, and the API is subject to change.

## ir_process_create

```c
ir_status_t ir_process_create(ir_handle_t *process, ir_handle_t *v_addr_region)
```

Creates a new process object and returns a handle to it. The program can use other syscalls to set it up as needed and then begin execution with `ir_process_start`

### Returns

Returns `IR_OK` to signal successful process creation, and places the handles in the locations pointed to by `process` and `v_addr_region`.

### Errors

- `IR_ERROR_INVALID_ARGUMENTS` if `process` or `v_addr_region` are not valid user pointers

## ir_thread_start

```c
ir_status_t ir_thread_start(ir_handle_t thread, uintptr_t entry, uintptr_t stack_top, uintptr_t arg0)
```

Start execution of a new process created with `ir_thread_create`.

### Returns

Returns `IR_OK` to signal successful process creation, and the thread begins executing from the given entry point.

## ir_process_exit

Ends a process with the given return code.

### Returns

This call does not return.
