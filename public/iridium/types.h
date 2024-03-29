/// @file public/iridium/types.h
/// @brief User-facing types and definitions for kernel objects and APIs

#ifndef PUBLIC_IRIDIUM_TYPES_H_
#define PUBLIC_IRIDIUM_TYPES_H_

// This null handle ID is never valid
#define IR_HANDLE_INVALID 0
#define THIS_PROCESS_HANDLE 1
#define ROOT_V_ADDR_REGION_HANDLE 2
#define STARTUP_CHANNEL_HANDLE 3


// Handle rights
#define IR_RIGHT_DUPLICATE 0x1 /// Right to create new copies of a handle
#define IR_RIGHT_TRANSFER 0x2 /// Right to send a handle to another process
#define IR_RIGHT_READ 0x4
#define IR_RIGHT_WRITE 0x8
#define IR_RIGHT_MAP 0x10 /// Right to map an object into memory
#define IR_RIGHT_EXECUTE 0x20 /// Right to map a vm object as executable
#define IR_RIGHT_INFO 0x40
#define IR_RIGHT_DESTORY 0x80 /// The right to perform destructive operations like killing tasks
#define IR_RIGHT_OP_CHILDREN 0x100 /// Right to perform operations that modify child objects

#define IR_RIGHT_ALL 0x1ff // Grant every right

// Represents the type of an object
#define OBJECT_TYPE_V_ADDR_REGION 1
#define OBJECT_TYPE_VM_OBJECT 2
#define OBJECT_TYPE_PROCESS 3
#define OBJECT_TYPE_THREAD 4
#define OBJECT_TYPE_TASK 5
#define OBJECT_TYPE_CHANNEL 6
#define OBJECT_TYPE_INTERRUPT 7
#define OBJECT_TYPE_IOPORT 8

#define V_ADDR_REGION_READABLE 0x1 // Can only be false if the target supports execute only pages
#define V_ADDR_REGION_WRITABLE 0x2
#define V_ADDR_REGION_EXECUTABLE 0x4
#define V_ADDR_REGION_MAP_SPECIFIC 0x8
#define V_ADDR_REGION_DISABLE_CACHE 0x10 /// Disable caching and use write-through

#define VM_READABLE 0x1
#define VM_WRITABLE 0x2
#define VM_EXECUTABLE 0x4
#define VM_DISABLE_CACHING 0x8

/// Disable caching and executation for mmio ranges
#define VM_MMIO_FLAGS (VM_DISABLE_CACHING | VM_WRITABLE | VM_READABLE)

// IO Port data sizes
#define SIZE_BYTE 0
#define SIZE_WORD 1
#define SIZE_LONG 2
#define SIZE_QUAD 3 /// Not supported on x86_64

/// Set when the process exits. Exit code is available
/// to read and discarding the handle is recomended
#define PROCESS_SIGNAL_TERMINATED 0x1
/// Set when the thread exits
#define THREAD_SIGNAL_TERMINATED 0x1

/// There is data ready to be read from the channel
#define CHANNEL_SIGNAL_DATA_WAITING 0x1
/// There is a handle ready to be received from the channel
#define CHANNEL_SIGNAL_HANDLE_WAITING 0x2
/// The channel cannot take in more messages
#define CHANNEL_SIGNAL_DATA_QUEUE_FULL 0x4
/// The channel cannot take in any more handles
#define CHANNEL_SIGNAL_HANDLE_QUEUE_FULL 0x8
/// The other end of the channel is gone, and can no longer send or recieve messages
#define CHANNEL_SIGNAL_PEER_DISCONNECTED 0x10

/// @brief Return status of all system calls and many internal functions.
/// A value of 0 (`IR_OK`) represents success, and error codes are negative values.
/// @see `public/iridium/errors.h` for error code definitions
typedef int ir_status_t;
/// Bitfield of the rights a handle grants
typedef unsigned long ir_rights_t;
/// Type used for handle ids
typedef unsigned long ir_handle_t;
/// Bit field of an object's currently active signals
typedef unsigned long ir_signal_t;

/// Data recieved when a signal is sent to an object
typedef struct {
    ir_status_t status;
    /// The bits for the signal(s) that set off the packet set to 1
    ir_signal_t trigger;
    /// The state of all of the object's signals
    ir_signal_t signals;

} ir_signal_packet_t;

#endif // PUBLIC_IRIDIUM_TYPES_H_
