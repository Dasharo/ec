// SPDX-License-Identifier: GPL-3.0-only

#ifndef _ARCH_TIME_H
#define _ARCH_TIME_H

#include <stdbool.h>
#include <stdint.h>

void time_init(void);
uint32_t time_get(void);

// Set every 50 ms by timer_0; cleared by the main-loop periodic handler
extern volatile bool timer_50ms_pending;

#endif // _ARCH_TIME_H
