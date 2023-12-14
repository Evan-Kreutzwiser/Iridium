
# Iridium Kernel

Iridium is a microkernel taking inspiration from Google's Fuchsia. The kernel manages resources using objects representations and uses a capability system for permissions.

It is responsible primarily for the following tasks, with the rest offset to userspace:
- Memory management (Swapping to disk is offset to userspace)
- IPC (Streams and rpc interfaces)
- Processes / Scheduling
- Hardware Interface for drivers (Interrupts and I/O)
    - Drivers exist in userspace, the kernel only controls IO

