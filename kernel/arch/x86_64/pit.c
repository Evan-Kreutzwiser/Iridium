/// @file arch/x86_64/pit.c
/// @brief Controls the Programable Interval Timer, used to calibrate more precise timers

#include "arch/x86_64/pit.h"
#include "arch/x86_64/asm.h"
#include "arch/x86_64/idt.h"
#include "kernel/devices/framebuffer.h"
#include <stdbool.h>

/// Frequency of the PIT in Hz. This number is divided to generate the target frequency
#define PIT_BASE_FREQUENCY 1193182
#define PIT_CHANNEL_0_PORT 0x40
#define PIT_CHANNEL_1_PORT 0x41
#define PIT_CHANNEL_2_PORT 0x42
#define PIT_COMMAND_PORT 0x43

volatile bool oneshot_triggered = 0;

extern char timer_calibration_irq;

void pit_init() {

    // Set channel 0 to mode 0 (interrupt on terminal count)
    // Set access mode such that 2 writes to the channel port set the bytes of the reload counter
    out_port_b(PIT_COMMAND_PORT, 3 << 4);
    idt_set_entry(33, 0x8, (uintptr_t)&timer_calibration_irq, IDT_GATE_INTERRUPT, 0);
    // Need APIC to redirect PIT line to interrupt table
}


void pit_one_shot(int ms) {
    oneshot_triggered = 0;
    int divisor = (PIT_BASE_FREQUENCY / 1000) * ms;
    out_port_b(PIT_CHANNEL_0_PORT, divisor & 0xff);
    out_port_b(PIT_CHANNEL_0_PORT, (divisor >> 8) & 0xff);

    while (!oneshot_triggered) {
        hlt();
    }
}
