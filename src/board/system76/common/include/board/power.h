// SPDX-License-Identifier: GPL-3.0-only

#ifndef _BOARD_POWER_H
#define _BOARD_POWER_H

#include <stdbool.h>

enum PowerState {
    POWER_STATE_S0 = 0x0,
    POWER_STATE_S3 = 0x3,
    POWER_STATE_S5 = 0x5,
    POWER_STATE_OFF = 0xf,
};

extern enum PowerState power_state;
void update_power_state(void);

#if USE_S0IX
#define PEP_S0IX_FLAG BIT(0)
#define PEP_DISPLAY_FLAG BIT(1)
extern uint8_t pep_hook;
#endif

void power_init(void);
void power_on(void);
void power_off(void);
void power_cpu_reset(void);
void power_apply_limit(bool ac);

void power_event(void);

#endif // _BOARD_POWER_H
