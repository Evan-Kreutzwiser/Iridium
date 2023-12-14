
#include <arch/defines.h>

// GDT for a 64 bit, flat memory model 
.section .data
.align 16

.global gdt
.type gdt, @object
.size gdt, gdt_end - gdt
gdt:

// Using this specific arrangement of segments facilitates the use of syscall/sysret
gdt_null_descriptor:
.word 0x0 // Limit low
.word 0x0 // Base low
.byte 0x0 // Base middle
.byte 0x0 // Access
.byte 0x0 // Limit high and flags
.byte 0x0 // Base high
gdt_kernel_code:
.word 0xffff // Limit low
.word 0x0 // Base low
.byte 0x0 // Base middle
.byte 0b10011010 // Access (Kernel, Code, Execute/Read)
.byte 0b10101111 // Limit high and flags (64 bit enabled)
.byte 0x0 // Base high
gdt_kernel_data:
.word 0xffff // Limit low
.word 0x0 // Base low
.byte 0x0 // Base middle
.byte 0b10010010 // Access (Kernel, Data, Read/Write)
.byte 0b10001111 // Limit high and flags
.byte 0x0 // Base high
gdt_user_data:
.word 0xffff // Limit low
.word 0x0 // Base low
.byte 0x0 // Base middle
.byte 0b11110010 // Access (User, Data, Read/Write)
.byte 0b10001111 // Limit high and flags
.byte 0x0 // Base high
gdt_user_code:
.word 0xffff // Limit low
.word 0x0 // Base low
.byte 0x0 // Base middle
.byte 0b11111010 // Access (User, Code, Execute/Read)
.byte 0b10101111 // Limit high and flags (64 bit enabled)
.byte 0x0 // Base high

// TSS selectors for ever cpu
gdt_tss_selectors:
.rept MAX_CPUS_COUNT

.word 0x0 // Limit low
.word 0x0 // Base low
.byte 0x0 // Base middle
.byte 0b00001001 // Access
.byte 0b00000000 // Limit high and flags 
.byte 0x0 // Base high

.long 0x00000000 // base highest
.long 0x00000000 // Reserved

.endr
gdt_end:

.global gdt_pointer_32
.type   gdt_pointer_32, @object
.align  16
gdt_pointer_32: // Pointer for the jump to long mode
.word gdt_end - gdt - 1             // Limit
.long gdt - KERNEL_VIRTUAL_ADDRESS  // Base physical address

.global gdt_pointer_64
.type   gdt_pointer_64, @object
.align  16
gdt_pointer_64: // Pointer for 64 bit mode
.word gdt_end - gdt - 1 // Limit
.quad gdt               // Base physical address
