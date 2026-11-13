// SPDX-License-Identifier: GPL-3.0-only

#ifndef _BOARD_FAN_H
#define _BOARD_FAN_H

#include <stdbool.h>
#include <stdint.h>

#define PWM_DUTY(X) ((uint8_t)(((((uint16_t)(X)) * 255) + 99) / 100))
#define MAX_FAN_SPEED PWM_DUTY(100)
#define MIN_FAN_SPEED PWM_DUTY(0)

#ifndef SMOOTH_FANS
#define SMOOTH_FANS 1 // default to fan smoothing
#endif

#ifndef SYNC_FANS
#define SYNC_FANS 1 // default to syncing fan speeds
#endif

#if SMOOTH_FANS != 0
#ifndef SMOOTH_FANS_UP
#define SMOOTH_FANS_UP 45 // default to ~11 seconds for full ramp-up
#endif
#ifndef SMOOTH_FANS_DOWN
#define SMOOTH_FANS_DOWN 100 // default to ~25 seconds for full ramp-down
#endif
#endif

#ifndef SMOOTH_FANS_MIN
#define SMOOTH_FANS_MIN 0 // default to smoothing all fan speed changes
#endif

#ifndef CPU_FAN1
#define CPU_FAN1 2
#endif

// Only V5x0TNx is different
#ifndef GPU_FAN1
#define GPU_FAN1 4
#endif

#define FAN1_SUPPORTED 1

#if defined(CPU_FAN2)
#define FAN2_SUPPORTED 1
#define FAN2_PWM CPU_FAN2
#elif HAVE_DGPU
#define FAN2_SUPPORTED 1
#define FAN2_PWM GPU_FAN1
#else
#define FAN2_SUPPORTED 0
#endif

struct FanPoint {
    int16_t temp;
    uint8_t duty;
};

struct Fan {
    struct FanPoint *points;
    uint8_t points_size;
    uint8_t *heatup;
    uint8_t heatup_size;
    uint8_t *cooldown;
    uint8_t cooldown_size;
    bool interpolate;
};

// Fans under manual control as in ACPI 0xDB
// bit 0 is CPU_FAN1, bit 1 is CPU_FAN2 or GPU_FAN1
#define FAN_MANUAL_1 (1 << 0)
#define FAN_MANUAL_2 (1 << 1)

// Fan curve wire format, as in CMD_FAN_CURVE_SET:
// four points of temperature in degrees C and duty in percent
#define FAN_CURVE_POINTS 4
#define FAN_CURVE_BYTES (2 * FAN_CURVE_POINTS)

// Fan a curve belongs to, as in the fan curve commands
// FAN1 is CPU_FAN1, FAN2 is CPU_FAN2 or GPU_FAN1
#define FAN_CURVE_FAN1 0
#define FAN_CURVE_FAN2 1
#define FAN_CURVE_COUNT 2

#define FAN_CURVE_APPLIED 0
#define FAN_CURVE_PENDING 1

extern bool fan_max;
extern uint8_t fan_manual;
// Curves being written, applied by fan_curve_commit()
extern uint8_t fan_curve_pending[FAN_CURVE_COUNT][FAN_CURVE_BYTES];
// Curves the fans run, read back with CMD_FAN_CURVE_GET
extern uint8_t fan_curve_applied[FAN_CURVE_COUNT][FAN_CURVE_BYTES];

void fan_reset(void);

bool fan_curve_is_present(uint8_t fan) __reentrant;
int16_t fan_curve_commit(uint8_t fan) __reentrant;
uint8_t fan_duty(const struct Fan *fan, int16_t temp) __reentrant;
void fan_duty_set(uint8_t fan1_duty, uint8_t fan2_duty) __reentrant;
uint8_t fan_heatup(const struct Fan *fan, uint8_t duty) __reentrant;
uint8_t fan_cooldown(const struct Fan *fan, uint8_t duty) __reentrant;
uint8_t fan_smooth(uint8_t last_duty, uint8_t duty) __reentrant;
uint8_t fan_points_are_valid(uint8_t count, struct FanPoint *points) __reentrant;

#endif // _BOARD_FAN_H
