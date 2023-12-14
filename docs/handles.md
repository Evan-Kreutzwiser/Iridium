# Handles

Handles are how programs access kernel resources and ipc. Programs are free to grant others access to handles and their associated resources, and reduce the permissions of their handles, but not increase their own permissions.

Handles are comprised of a user-facing ID, a reference to an object, and a set of rights.

## Handle IDs

Each handle is granted an ID that is unique within the current process. Handle IDs do not corespond with handles to resources in other processes, and thus shuold not be shared over ipc. There exists another mechanism if handle sharing is required.

User processes can store handle as `ir_handle_t` variables, which contain the ID. This ID can be passed to system calls to access or manipulate the resources.

### Null ID

ID 0 is reserved as a 'null' handle or invalid handle. This handle cannot be used for any purpose, and attempting to use it in a system call will return `IR_ERROR_BAD_HANDLE`.

## Default Handles

All processes begin with 2 default handles. Handle ID 1 in any process refers to its own process, and handle ID 2 refers to the root `v_addr_region`, which covers the entire user-accessible address space. All of the process's code and data are mapped as child `v_addr_region`s under this root, and it can only be destroyed by terminating the process.
