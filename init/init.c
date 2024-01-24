
#include "iridium/syscalls.h"
#include "iridium/types.h"
#include "iridium/errors.h"

const char keys[] = {
    [0x2] = '1',
    [0x3] = '2',
    [0x4] = '3',
    [0x5] = '4',
    [0x6] = '5',
    [0x7] = '6',
    [0x8] = '7',
    [0x9] = '8',
    [0xa] = '9',
    [0xb] = '0',
    [0xc] = '-',
    [0xd] = '=',

    [0xf] = '\t',
    [0x10] = 'Q',
    [0x11] = 'W',
    [0x12] = 'E',
    [0x13] = 'R',
    [0x14] = 'T',
    [0x15] = 'Y',
    [0x16] = 'U',
    [0x17] = 'I',
    [0x18] = 'O',
    [0x19] = 'P',
    [0x1a] = '[',
    [0x1b] = ']',
    [0x1c] = '\n',
    [0x1d] = 0,
    [0x1e] = 'A',
    [0x1f] = 'S',
    [0x20] = 'D',
    [0x21] = 'F',
    [0x22] = 'G',
    [0x23] = 'H',
    [0x24] = 'J',
    [0x25] = 'K',
    [0x26] = 'L',
    [0x27] = ';',
    [0x28] = '\'',
    [0x29] = '`',
    [0x2a] = 0,
    [0x2b] = '\\',
    [0x2c] = 'Z',
    [0x2d] = 'X',
    [0x2e] = 'C',
    [0x2f] = 'V',
    [0x30] = 'B',
    [0x31] = 'N',
    [0x32] = 'M',
    [0x33] = ',',
    [0x34] = '.',
    [0x35] = '/',
    [0x36] = 0,
    [255] = 0
};

static inline ir_status_t syscall_1(long syscall_num, long arg0) {
    ir_status_t result;
    asm volatile ("syscall" : "=a" (result) : "D" (syscall_num), "S" (arg0) : "r8", "r9", "r10", "r11", "rcx", "rcx");
    return result;
}

static inline ir_status_t syscall_2(long syscall_num, long arg0, long arg1) {
    ir_status_t result;
    asm volatile ("syscall" : "=a" (result) : "D" (syscall_num), "S" (arg0), "d" (arg1) : "r8", "r9", "r10", "r11", "rcx");
    return result;
}

static inline ir_status_t syscall_3(long syscall_num, long arg0, long arg1, long arg2) {
    ir_status_t result;
    register long r10 asm("r10") = arg2;
    asm volatile ("syscall" : "=a" (result) : "D" (syscall_num), "S" (arg0), "d" (arg1), "r" (r10) : "r8", "r9", "r11", "rcx");
    return result;
}

static inline ir_status_t syscall_4(long syscall_num, long arg0, long arg1, long arg2, long arg3) {
    ir_status_t result;
    register long r10 asm("r10") = arg2;
    register long r8 asm("r8") = arg3;
    asm volatile ("syscall" : "=a" (result) : "D" (syscall_num), "S" (arg0), "d" (arg1), "r" (r10), "r" (r8) :  "r9", "r11", "rcx");
    return result;
}

static inline ir_status_t syscall_5(long syscall_num, long arg0, long arg1, long arg2, long arg3, long arg4) {
    ir_status_t result;
    register long r10 asm("r10") = arg2;
    register long r8 asm("r8") = arg3;
    register long r9 asm("r9") = arg4;
    asm volatile ("syscall" : "=a" (result) : "D" (syscall_num), "S" (arg0), "d" (arg1), "r" (r10), "r" (r8), "r" (r9) : "r11", "rcx");
    return result;
}

void sys_print(const char *str) {
    syscall_1(SYSCALL_SERIAL_OUT, (long)str);
}

ir_status_t handle_close(ir_handle_t handle) {
    return syscall_1(SYSCALL_HANDLE_CLOSE, (long)handle);
}

ir_status_t v_addr_region_map(ir_handle_t parent, ir_handle_t vm_object, uint64_t flags, ir_handle_t *region_out, void *address_out) {
    return syscall_5(SYSCALL_V_ADDR_REGION_MAP, parent, vm_object, flags, (long)region_out, (long)address_out);
}

ir_status_t vm_object_create(unsigned long size, uint64_t flags, ir_handle_t *out) {
    return syscall_3(SYSCALL_VM_OBJECT_CREATE, size, flags, (long)out);
}

ir_status_t get_framebuffer(ir_handle_t *fb_handle, int *width, int *height, int *pitch, int *bpp) {
    return syscall_5(SYSCALL_DEBUG_GET_FRAMEBUFFER, (long)fb_handle, (long)width, (long)height, (long)pitch, (long)bpp);
}

ir_handle_t framebuffer_handle;
ir_handle_t region_handle;
unsigned char *framebuffer;
int width;
int height;
int pitch;
int bpp;

int wait() {
    int x = 9;
    for (int i = 0; i < 10000; i++) {
        x--;
    }
    return x;
}

void spawn_thread(void *entry_pointer)
{
    ir_handle_t stack_vmo = 0xDEADDEAD;
    ir_status_t status = vm_object_create(4096 * 8, VM_READABLE | VM_WRITABLE, &stack_vmo);
    if (status) {
        syscall_2(SYSCALL_SERIAL_OUT, (long)"Error %d Creating stack vmo\n", status);
        syscall_1(SYSCALL_DEBUG_DUMP_HANDLES, 0);
    }

    uintptr_t stack_base = 0xDEADDEAD;
    ir_handle_t region;
    status = v_addr_region_map(ROOT_V_ADDR_REGION_HANDLE, stack_vmo, V_ADDR_REGION_READABLE | V_ADDR_REGION_WRITABLE, &region, &stack_base);
    if (status) {
        syscall_2(SYSCALL_SERIAL_OUT, (long)"Error %d Mapping thread stack\n", status);
        syscall_1(SYSCALL_DEBUG_DUMP_HANDLES, 0);
    }

    uintptr_t stack_top = stack_base + (4096 * 8) - 16;

    ir_handle_t thread;
    syscall_2(SYSCALL_THREAD_CREATE, THIS_PROCESS_HANDLE, (unsigned long)&thread);
    syscall_4(SYSCALL_THREAD_START, thread, (unsigned long)entry_pointer, stack_top, 0);
}

// 5 ports at vector 60
ir_handle_t ps2_ports;
#define DATA_PORT_OFFSET 0
#define COMMAND_PORT_OFFSET 4
#define STATUS_PORT_OFFSET 4

void outportb(ir_handle_t ports, int offset, unsigned char value) {
    ir_status_t status = syscall_4(SYSCALL_IOPORT_SEND, ports, offset, value, SIZE_BYTE);
    if (status)
        syscall_3(SYSCALL_SERIAL_OUT, (long)"Error %d writing port offset %d\n", status, offset);
}

long inportb(ir_handle_t ports, int offset) {
    long value = 0;
    ir_status_t status = syscall_4(SYSCALL_IOPORT_RECEIVE, ports, offset, SIZE_BYTE, (long)&value);
    if (status)
        syscall_3(SYSCALL_SERIAL_OUT, (long)"Error %d reading port offset %d\n", status, offset);
    return value;
}

char keyboard_read() {

    int attempts = 1000;

    while(attempts--) {
        if (inportb(ps2_ports, STATUS_PORT_OFFSET) & 1){
            inportb(ps2_ports, DATA_PORT_OFFSET);
        }
    }

    return 'e'; // Error character
}

char keyboard_write(unsigned char value) {
    int attempts = 100000;

    while(attempts--) {
        if (!(inportb(ps2_ports, STATUS_PORT_OFFSET) & 2)) {
            outportb(ps2_ports, DATA_PORT_OFFSET, value);
        }
    }

    // Return the ack or response byte
    return keyboard_read();
}

void keyboard_thread() {

    sys_print("Starting keyboard thread\n");

    ir_status_t status = syscall_3(SYSCALL_IOPORT_CREATE, 60, 5, (long)&ps2_ports);
    if (status) {
        syscall_2(SYSCALL_SERIAL_OUT, (long)"Error %d getting ports\n", status);
    }

    long value = 0;

    // Disable other devices that might interfere with setup
    outportb(ps2_ports, COMMAND_PORT_OFFSET, 0xad);
    outportb(ps2_ports, COMMAND_PORT_OFFSET, 0xa7);

    // Clear the input buffer (if applicable) by reading the port and discarding the value
    keyboard_read();

    outportb(ps2_ports, DATA_PORT_OFFSET, 0x20);

    // Enable ps2 port 1 interrupts and translation to scancode set 1
    syscall_4(SYSCALL_IOPORT_SEND, ps2_ports, DATA_PORT_OFFSET, 0x60, SIZE_BYTE);
    outportb( ps2_ports, DATA_PORT_OFFSET, (value | 1 | (1 << 6)));

    outportb(ps2_ports, COMMAND_PORT_OFFSET, 0xae);

    value = keyboard_write(0xf4);
    syscall_2(SYSCALL_SERIAL_OUT, (long)"%c", value);

    ir_handle_t interrupt;
    status = syscall_2(SYSCALL_INTERRUPT_CREATE, 34, (long)&interrupt);
    if (status) {
        syscall_2(SYSCALL_SERIAL_OUT, (long)"Error %d registering interrupt\n", status);
    }


    int position = pitch * 20 + (bpp/8 * 128);

    while (1) {
        // Wait for interrupt
        status = syscall_1(SYSCALL_INTERRUPT_WAIT, interrupt);
        if (status) {
            syscall_2(SYSCALL_SERIAL_OUT, (long)"Error %d waiting for interrupt\n", status);
            syscall_1(SYSCALL_DEBUG_DUMP_HANDLES, 0);
            while (1) {}
        }

        sys_print("Int");
        // Read byte
        value = inportb(ps2_ports, DATA_PORT_OFFSET);
        syscall_2(SYSCALL_SERIAL_OUT, (long)"%c", keys[value]);
        syscall_2(SYSCALL_SERIAL_OUT, (long)"(%x)  ", value);
        while (value != 'e') {
            value = keyboard_read();
            syscall_2(SYSCALL_SERIAL_OUT, (long)"(%x)", value);
        }
        for (int i = 0; i < 64; i++, position += pitch) {
            for (int j = 0; j < 64; j++) {
                framebuffer[position + (j * (bpp / 8))] = 128;
                framebuffer[position + (j * (bpp / 8)) + 1] = 255 - i;
                framebuffer[position + (j * (bpp / 8)) + 2] = j;
            }
        }
        position += bpp * 64;
    }
}


void thread_entry() {

    sys_print("Spawned new thread\n");

    //for (int i = 0; i < 64; i++) {
        //syscall_1(SYSCALL_YIELD, 0);
    //}

    int x = 0;
    while (1) {
        int position = pitch * 164 + (bpp/8 * 664);
        for (int i = 0; i < 64; i++, position += pitch) {
            for (int j = 0; j < 64; j++) {
                framebuffer[position + (j * (bpp / 8))+1] = x;
                framebuffer[position + (j * (bpp / 8))] = i*4;
                framebuffer[position + (j * (bpp / 8)) + 2] = 255 - j*4;
            }
        }
        x = (x + 1) % 128;
    }
}

void _start(void) {
    sys_print("--------\nHello from the init process!\n--------\n");

    ir_status_t status = get_framebuffer(&framebuffer_handle, &width, &height, &pitch, &bpp);
    if (status == IR_OK) {
        status = v_addr_region_map(ROOT_V_ADDR_REGION_HANDLE, framebuffer_handle, V_ADDR_REGION_READABLE | V_ADDR_REGION_WRITABLE, &region_handle, &framebuffer);
        if (status == IR_OK ) {
            syscall_2(SYSCALL_SERIAL_OUT, (long)"Framebuffer successfully mapped to %#p\n", (long)framebuffer);
            int position = pitch * 100 + (bpp/8 * 600);
            for (int i = 0; i < 64; i++, position += pitch) {
                for (int j = 0; j < 64; j++) {
                    framebuffer[position + (j * (bpp / 8))] = 128;
                    framebuffer[position + (j * (bpp / 8)) + 1] = i*4;
                    framebuffer[position + (j * (bpp / 8)) + 2] = 255 - j*4;
                }
            }

            spawn_thread(thread_entry);

            spawn_thread(keyboard_thread);

            int x = 0;
            while (1) {
                position = pitch * 100 + 664 * bpp/8;
                for (int i = 0; i < 64; i++, position += pitch) {
                    for (int j = 0; j < 64; j++) {
                        framebuffer[position + (j * (bpp / 8))] = x;
                        framebuffer[position + (j * (bpp / 8)) + 1] = i*4;
                        framebuffer[position + (j * (bpp / 8)) + 2] = 255 - j*4;
                    }
                }
                x = (x + 1) % 128;
            }
        }
        else {
            syscall_2(SYSCALL_SERIAL_OUT, (long)"Mapping framebuffer failed with code %d\n", status);
        }
    }
    else {
        sys_print("Could not get framebuffer\n");
    }

    while (1) {
        //syscall_1(SYSCALL_YIELD, 0);
    }
}
