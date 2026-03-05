// SPDX-License-Identifier: GPL-3.0-only

// Uses timer 0 to keep track of global time

#include <8051.h>

#include <arch/time.h>

static volatile uint32_t time_overflows = 0;

// Set every 50 ms by timer_0; cleared by main loop periodic handler.
// 50 ms is the base for all periodic intervals:
//   2 ticks = 100 ms (power/usbpd poll)
//   5 ticks = 250 ms (smooth fan)
//  20 ticks = 1000 ms (fan normal + battery)
volatile bool timer_50ms_pending = false;

void timer_0(void) __interrupt(1) {
    // Stop timer
    TR0 = 0;

    time_overflows++;

    static uint8_t ticks = 0;
    if (++ticks >= 50) {
        ticks = 0;
        timer_50ms_pending = true;
    }

    // Start timer
    TH0 = 0xFD;
    TL0 = 0x01;
    TR0 = 1;
}

void time_init(void) __critical {
    // Stop the timer
    TR0 = 0;
    TF0 = 0;

    time_overflows = 0;

    // Enable timer interrupts
    ET0 = 1;

    // Start timer in mode 1
    // (65536 - 64769) / (9.2 MHz / 12) = ~1 ms interval
    TMOD = (TMOD & 0xF0) | 0x01;
    TH0 = 0xFD;
    TL0 = 0x01;
    TR0 = 1;
}

uint32_t time_get(void) __critical {
    return time_overflows;
}
