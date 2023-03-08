// SPDX-License-Identifier: GPL-3.0-only

#ifndef _BOARD_POWER_H
#define _BOARD_POWER_H

#include <stdbool.h>

enum PowerState {
    POWER_STATE_OFF,
    POWER_STATE_S5,
    POWER_STATE_S3,
    POWER_STATE_S0,
};

enum SleepType {
    SLEEP_TYPE_S0IX,
    SLEEP_TYPE_S3,
};

extern enum PowerState power_state;

#if EC_ESPI
extern bool in_s0ix;
extern bool use_s0ix;
#endif

void power_init(void);
void power_on(void);
void power_off(void);
void power_set_limit(void);
void power_cpu_reset(void);

void power_event(void);

bool power_is_s0ix_enabled(void);
void power_set_sleep_type(enum SleepType slp_type);

#endif // _BOARD_POWER_H
