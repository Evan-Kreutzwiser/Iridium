
#include "iridium/syscalls.h"
#include "iridium/types.h"
#include "iridium/errors.h"
#include <stdbool.h>
#include <stdint.h>

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
    [0xe] = 8, // Backspace
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
    [0x37] = '*', // Numpad
    [0x38] = 0, // Left alt
    [0x39] = ' ',
    [0x47] = '7', // Numpad
    [0x48] = '8', // Numpad
    [0x49] = '9', // Numpad
    [0x4a] = '-', // Numpad
    [0x4b] = '4', // Numpad
    [0x4c] = '5', // Numpad
    [0x4d] = '6', // Numpad
    [0x4e] = '+', // Numpad
    [0x4f] = '1', // Numpad
    [0x50] = '2', // Numpad
    [0x51] = '3', // Numpad
    [0x52] = '0', // Numpad
    [0x53] = '.', // Numpad
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
        syscall_2(SYSCALL_SERIAL_OUT, (long)"Error %d Creating stack vmo - failed to spawn thread\n", status);
        syscall_1(SYSCALL_DEBUG_DUMP_HANDLES, 0);
        return;
    }

    uintptr_t stack_base = 0xDEADDEAD;
    ir_handle_t region;
    status = v_addr_region_map(ROOT_V_ADDR_REGION_HANDLE, stack_vmo, V_ADDR_REGION_READABLE | V_ADDR_REGION_WRITABLE, &region, &stack_base);
    if (status) {
        syscall_2(SYSCALL_SERIAL_OUT, (long)"Error %d Mapping thread stack - failed to spawn thread\n", status);
        syscall_1(SYSCALL_DEBUG_DUMP_HANDLES, 0);
        return;
    }

    uintptr_t stack_top = stack_base + (4096 * 8) - 16;

    ir_handle_t thread;
    syscall_2(SYSCALL_THREAD_CREATE, THIS_PROCESS_HANDLE, (unsigned long)&thread);
    syscall_4(SYSCALL_THREAD_START, thread, (unsigned long)entry_pointer, stack_top, 0);
}

void spawn_thread_and_wait_for_exit(void *entry_pointer)
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

    ir_signal_t observed_signals = 0;
    status = syscall_4(SYSCALL_OBJECT_WAIT, thread, THREAD_SIGNAL_TERMINATED, -1, (long)&observed_signals);

    syscall_3(SYSCALL_SERIAL_OUT, (long)"Status %d from wait for thread termination - signals: %#x\n", status, observed_signals);
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

/// Wait for the ps/2 controller to have a byte for us to read
#define WAIT_FOR_OUTPUT_FULL() while(!(inportb(ps2_ports, STATUS_PORT_OFFSET) & 1))
/// Wait for the ps/2 controller to be ready for additional input
#define WAIT_FOR_INPUT_CLEAR() while(inportb(ps2_ports, STATUS_PORT_OFFSET) & 2)

char keyboard_read() {

    int attempts = 1000;

    while(attempts--) {
        if (inportb(ps2_ports, STATUS_PORT_OFFSET) & 1){
            inportb(ps2_ports, DATA_PORT_OFFSET);
        }
    }

    return 'e'; // Error character
}

static inline void keyboard_write(unsigned char value) {
    WAIT_FOR_INPUT_CLEAR();
    outportb(ps2_ports, DATA_PORT_OFFSET, value);
}

void keyboard_thread() {

    sys_print("Starting keyboard thread\n");

    bool is_dual_channel = true;

    ir_status_t status = syscall_3(SYSCALL_IOPORT_CREATE, 0x60, 5, (long)&ps2_ports);
    if (status) {
        syscall_2(SYSCALL_SERIAL_OUT, (long)"Error %d getting ports\n", status);
    }

    long value = 0;

    // Disable other devices that might interfere with setup
    WAIT_FOR_INPUT_CLEAR();
    outportb(ps2_ports, COMMAND_PORT_OFFSET, 0xad);
    WAIT_FOR_INPUT_CLEAR();
    outportb(ps2_ports, COMMAND_PORT_OFFSET, 0xa7);

    // Clear the input buffer (if applicable) by reading the port and discarding the value
    int s = inportb(ps2_ports, STATUS_PORT_OFFSET);
    while (s & 1) {
        sys_print("Keyboard had data waiting\n");
        inportb(ps2_ports, DATA_PORT_OFFSET);
        s = inportb(ps2_ports, STATUS_PORT_OFFSET);
    }

    // Configure ps2 controller for initialization
    WAIT_FOR_INPUT_CLEAR();
    outportb(ps2_ports, COMMAND_PORT_OFFSET, 0x20);
    WAIT_FOR_OUTPUT_FULL();
    int config = inportb(ps2_ports, DATA_PORT_OFFSET);
    config &= ~(3 | (1 << 6)); // Disable translation and interrupts
    if (config & (1 << 5)) { // Bit 5 indicates the second port's clock is disabled
        is_dual_channel = false;
    }
    WAIT_FOR_INPUT_CLEAR();
    outportb(ps2_ports, COMMAND_PORT_OFFSET, 0x60);
    WAIT_FOR_INPUT_CLEAR();
    outportb(ps2_ports, DATA_PORT_OFFSET, config);

    // Perform a self-test of the controller
    WAIT_FOR_INPUT_CLEAR();
    outportb(ps2_ports, COMMAND_PORT_OFFSET, 0xaa);
    WAIT_FOR_OUTPUT_FULL();
    int response = inportb(ps2_ports, DATA_PORT_OFFSET);
    if (response != 0x55) {
        syscall_2(SYSCALL_SERIAL_OUT, (long)"Cannot initialize keyboard - ps/2 controller failed self test (Returned %#x).\n", response);
        while (1);
    }

    sys_print("PS/2 self test passed\n");

    // In case the self test caused a reset, restore the configuration
    WAIT_FOR_INPUT_CLEAR();
    outportb(ps2_ports, COMMAND_PORT_OFFSET, 0x60);
    WAIT_FOR_INPUT_CLEAR();
    outportb(ps2_ports, DATA_PORT_OFFSET, config);

    // Test for the second port
    if (is_dual_channel) {
        WAIT_FOR_INPUT_CLEAR();
        outportb(ps2_ports, COMMAND_PORT_OFFSET, 0xa8); // Enable the second port

        // If the port
        WAIT_FOR_INPUT_CLEAR();
        outportb(ps2_ports, COMMAND_PORT_OFFSET, 0x20);
        WAIT_FOR_OUTPUT_FULL();
        int config = inportb(ps2_ports, DATA_PORT_OFFSET);
        if (config & (1 << 5)) { // Bit 5 indicates the second port's clock is disabled
            is_dual_channel = false;
            outportb(ps2_ports, COMMAND_PORT_OFFSET, 0xa7);
        } else { sys_print("Second PS/2 port present\n"); }
    }



    // Enable ps2 port 1 interrupts and translation to scancode set 1
    WAIT_FOR_INPUT_CLEAR();
    outportb(ps2_ports, COMMAND_PORT_OFFSET, 0x60);
    WAIT_FOR_INPUT_CLEAR();
    outportb(ps2_ports, DATA_PORT_OFFSET, (value | 1 | (1 << 6)));

    // Reset and self test
    WAIT_FOR_INPUT_CLEAR();
    outportb(ps2_ports, DATA_PORT_OFFSET, 0xFF);
    WAIT_FOR_OUTPUT_FULL();
    if (inportb(ps2_ports, DATA_PORT_OFFSET) != 0xFA) { sys_print("Keyboard didn't perform reset\n"); }
    WAIT_FOR_OUTPUT_FULL();
    value = inportb(ps2_ports, DATA_PORT_OFFSET);
    if (inportb(ps2_ports, DATA_PORT_OFFSET) != 0xAA) { sys_print("Keyboard self test failed\n"); }

    ir_handle_t interrupt;
    status = syscall_3(SYSCALL_INTERRUPT_CREATE, 34, 1, (long)&interrupt);
    if (status) {
        syscall_2(SYSCALL_SERIAL_OUT, (long)"Error %d registering interrupt\n", status);
    }

    // Enable interrupts for the first port
    WAIT_FOR_INPUT_CLEAR();
    outportb(ps2_ports, COMMAND_PORT_OFFSET, 0xae);

    keyboard_write(0xf4);
    WAIT_FOR_OUTPUT_FULL();
    value = inportb(ps2_ports, DATA_PORT_OFFSET);

    if (value == 0xfa) {
        sys_print("Keyboard ACKed interrupt enabling\n");
    } else {
        sys_print("Failed to enable interrupts\n");
    }

    //syscall_2(SYSCALL_SERIAL_OUT, (long)"%c", value);
    s = inportb(ps2_ports, STATUS_PORT_OFFSET);
    while (s & 2) {
        value = inportb(ps2_ports, DATA_PORT_OFFSET);
        syscall_2(SYSCALL_SERIAL_OUT, (long)"%c\n", value);
        s = inportb(ps2_ports, STATUS_PORT_OFFSET);
    }

    sys_print("Entering keyboard loop - press left alt to terminate init process\n");

    while (1) {
        // Wait for interrupt
        status = syscall_1(SYSCALL_INTERRUPT_WAIT, interrupt);
        if (status) {
            syscall_2(SYSCALL_SERIAL_OUT, (long)"Error %d waiting for interrupt\n", status);
            while (1) {}
        }
        syscall_1(SYSCALL_INTERRUPT_ARM, interrupt);

        s = inportb(ps2_ports, STATUS_PORT_OFFSET);
        while (s & 1) {
            value = inportb(ps2_ports, DATA_PORT_OFFSET);
            if (value == 0x38) {
                sys_print("\nEnding process.\n");

                extern void exit(int);
                exit(-4);
            }

            syscall_2(SYSCALL_SERIAL_OUT, (long)"%c", keys[value]);
            s = inportb(ps2_ports, STATUS_PORT_OFFSET);
        }
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

void sleeping_thread(void) {

    int r = 255;
    int g = 0;
    int b = 0;
    int changing_color = 0;
    while (1) {
        int position = pitch * 228 + 664 * bpp/8;
        for (int i = 0; i < 64; i++, position += pitch) {
            for (int j = 0; j < 64; j++) {
                framebuffer[position + (j * (bpp / 8))] = r;
                framebuffer[position + (j * (bpp / 8)) + 1] = g;
                framebuffer[position + (j * (bpp / 8)) + 2] = b;
            }
        }
        // Wait 10 milliseconds
        syscall_1(SYSCALL_SLEEP_MICROSECONDS, 10000);

        if (changing_color == 0) {
            r--;
            g++;
            if (g == 255) {
                changing_color = 1;
                // Wait 4 seconds
                syscall_1(SYSCALL_SLEEP_MICROSECONDS, 4000000);
            }
        } else if (changing_color == 1) {
            g--;
            b++;
            if (b == 255) {
                changing_color = 2;
                // Wait 4 seconds
                syscall_1(SYSCALL_SLEEP_MICROSECONDS, 4000000);
            }
        } else if (changing_color == 2) {
            b--;
            r++;
            if (r == 255) {
                changing_color = 0;
                // Wait 4 seconds
                syscall_1(SYSCALL_SLEEP_MICROSECONDS, 4000000);
            }
        }
    }

}

void thread_that_exits(void) {
    // 4 second
    syscall_1(SYSCALL_SLEEP_MICROSECONDS, 4000000);
    syscall_1(SYSCALL_THREAD_EXIT, -1);
}

int main(void) {
    sys_print("--------\nHello from the init process!\n--------\nWaiting for test thread to exit...\n");

    spawn_thread_and_wait_for_exit(thread_that_exits);

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

            spawn_thread(sleeping_thread);

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
