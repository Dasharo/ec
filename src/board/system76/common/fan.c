// SPDX-License-Identifier: GPL-3.0-only

#include <board/dgpu.h>
#include <board/fan.h>
#include <board/peci.h>
#include <common/debug.h>
#include <common/macro.h>
#include <ec/pwm.h>
#include <string.h>

#if SMOOTH_FANS != 0
#define MAX_JUMP_UP ((MAX_FAN_SPEED - MIN_FAN_SPEED) / (uint8_t)SMOOTH_FANS_UP)
#define MAX_JUMP_DOWN ((MAX_FAN_SPEED - MIN_FAN_SPEED) / (uint8_t)SMOOTH_FANS_DOWN)
#else
#define MAX_JUMP_UP (MAX_FAN_SPEED - MIN_FAN_SPEED)
#define MAX_JUMP_DOWN (MAX_FAN_SPEED - MIN_FAN_SPEED)
#endif

#define MIN_SPEED_TO_SMOOTH PWM_DUTY(SMOOTH_FANS_MIN)

#define PWM_REG(x) concat(DCR, x)

static bool fan_curves_match = true;

bool fan_max = false;
uint8_t fan_manual = 0;
uint8_t last_duty_fan1 = 0;
uint8_t last_duty_fan2 = 0;
uint8_t fan_curve_pending[FAN_CURVE_COUNT][FAN_CURVE_BYTES] = { { 0 } };
uint8_t fan_curve_applied[FAN_CURVE_COUNT][FAN_CURVE_BYTES] = { { 0 } };

void fan_reset(void) {
    // Do not manually set fans to maximum speed
    fan_max = false;
    fan_manual = 0;
}

bool fan_curve_is_present(uint8_t fan) __reentrant {
    switch (fan) {
    case FAN_CURVE_FAN1:
        return true;
#if FAN2_SUPPORTED
    case FAN_CURVE_FAN2:
        return true;
#endif
    }
    return false;
}

int16_t fan_curve_commit(uint8_t fan) __reentrant {
    struct FanPoint points[FAN_CURVE_POINTS];
    int16_t res = 0;
    uint8_t i;

    if (!fan_curve_is_present(fan)) {
        return -1;
    }

    for (i = 0; i < FAN_CURVE_POINTS; i++) {
        uint8_t duty = fan_curve_pending[fan][2 * i + 1];
        if (duty > 100) {
            res = -1;
        }
        points[i].temp = fan_curve_pending[fan][2 * i];
        points[i].duty = duty * 255 / 100;
    }

    if (res == 0 && !fan_points_are_valid(FAN_CURVE_POINTS, points)) {
        res = -1;
    }
    if (res == 0 && fan == FAN_CURVE_FAN1) {
        res = peci_set_fan_curve(FAN_CURVE_POINTS, points);
    }
#if HAVE_DGPU
    if (res == 0 && fan == FAN_CURVE_FAN2) {
        res = dgpu_set_fan_curve(FAN_CURVE_POINTS, points);
    }
#elif defined(CPU_FAN2)
    if (res == 0 && fan == FAN_CURVE_FAN2) {
        res = peci_set_fan2_curve(FAN_CURVE_POINTS, points);
    }
#endif

    if (res == 0) {
        // apply pending to applied
        memcpy(fan_curve_applied[fan], fan_curve_pending[fan], FAN_CURVE_BYTES);
        fan_curves_match = memcmp(
                               fan_curve_applied[FAN_CURVE_FAN1],
                               fan_curve_applied[FAN_CURVE_FAN2],
                               FAN_CURVE_BYTES
                           ) == 0;
    } else {
        // restore applied to pending
        memcpy(fan_curve_pending[fan], fan_curve_applied[fan], FAN_CURVE_BYTES);
    }
    return res;
}

// Get duty cycle based on temperature, adapted from
// https://github.com/pop-os/system76-power/blob/master/src/fan.rs
uint8_t fan_duty(const struct Fan *fan, int16_t temp) __reentrant {
    for (uint8_t i = 0; i < fan->points_size; i++) {
        const struct FanPoint *cur = &fan->points[i];

        // If exactly the current temp, return the current duty
        if (temp == cur->temp) {
            return cur->duty;
        } else if (temp < cur->temp) {
            // If lower than first temp, return 0%
            if (i == 0) {
                return MIN_FAN_SPEED;
            } else {
                const struct FanPoint *prev = &fan->points[i - 1];

                if (fan->interpolate) {
                    // If in between current temp and previous temp, interpolate
                    if (temp > prev->temp) {
                        int16_t dtemp = (cur->temp - prev->temp);
                        int16_t dduty = ((int16_t)cur->duty) - ((int16_t)prev->duty);
                        // clang-format off
                        return (uint8_t)(
                            ((int16_t)prev->duty) +
                            ((temp - prev->temp) * dduty) / dtemp
                        );
                        // clang-format on
                    }
                } else {
                    return prev->duty;
                }
            }
        }
    }

    // If no point is found, return 100%
    return MAX_FAN_SPEED;
}

void fan_duty_set(uint8_t fan1_duty, uint8_t fan2_duty) __reentrant {
    bool fan1_man = !!(fan_manual & FAN_MANUAL_1);
#if FAN2_SUPPORTED
    bool fan2_man = !!(fan_manual & FAN_MANUAL_2);

#if SYNC_FANS != 0
    if (fan_curves_match && !fan1_man && !fan2_man) {
        fan1_duty = fan1_duty > fan2_duty ? fan1_duty : fan2_duty;
        fan2_duty = fan1_duty;
    }
#endif
#else
    fan2_duty = fan2_duty; // unused if fan 2 is not supported
#endif

    // set fan 1 duty
    if (!fan1_man && fan1_duty != PWM_REG(CPU_FAN1)) {
        TRACE("FAN1 fan_duty_raw=%d\n", fan1_duty);
        last_duty_fan1 = fan1_duty = fan_smooth(last_duty_fan1, fan1_duty);
        PWM_REG(CPU_FAN1) = fan_max ? MAX_FAN_SPEED : fan1_duty;
        TRACE("FAN1 fan_duty_smoothed=%d\n", fan1_duty);
    }

#if FAN2_SUPPORTED
    // set fan 2 duty
    if (!fan2_man && fan2_duty != PWM_REG(FAN2_PWM)) {
        TRACE("FAN2 fan_duty_raw=%d\n", fan2_duty);
        last_duty_fan2 = fan2_duty = fan_smooth(last_duty_fan2, fan2_duty);
        PWM_REG(FAN2_PWM) = fan_max ? MAX_FAN_SPEED : fan2_duty;
        TRACE("FAN2 fan_duty_smoothed=%d\n", fan2_duty);
    }
#endif
}

uint8_t fan_heatup(const struct Fan *fan, uint8_t duty) __reentrant {
    uint8_t lowest = duty;

    uint8_t i;
    for (i = 0; (i + 1) < fan->heatup_size; i++) {
        uint8_t value = fan->heatup[i + 1];
        if (value < lowest) {
            lowest = value;
        }
        fan->heatup[i] = value;
    }
    fan->heatup[i] = duty;

    return lowest;
}

uint8_t fan_cooldown(const struct Fan *fan, uint8_t duty) __reentrant {
    uint8_t highest = duty;

    uint8_t i;
    for (i = 0; (i + 1) < fan->cooldown_size; i++) {
        uint8_t value = fan->cooldown[i + 1];
        if (value > highest) {
            highest = value;
        }
        fan->cooldown[i] = value;
    }
    fan->cooldown[i] = duty;

    return highest;
}

uint8_t fan_smooth(uint8_t last_duty, uint8_t duty) __reentrant {
    uint8_t next_duty = duty;

    // ramping down
    if (duty < last_duty) {
        // out of bounds (lower) safeguard
        // clang-format off
        uint8_t smoothed = last_duty < MIN_FAN_SPEED + MAX_JUMP_DOWN
            ? MIN_FAN_SPEED
            : last_duty - MAX_JUMP_DOWN;
        // clang-format on

        // use smoothed value if above min and if smoothed is closer than raw
        if (last_duty > MIN_SPEED_TO_SMOOTH && smoothed > duty) {
            next_duty = smoothed;
        }
    }

    // ramping up
    if (duty > last_duty) {
        // out of bounds (higher) safeguard
        // clang-format off
        uint8_t smoothed = last_duty > MAX_FAN_SPEED - MAX_JUMP_UP
            ? MAX_FAN_SPEED
            : last_duty + MAX_JUMP_UP;
        // clang-format on

        // use smoothed value if above min and if smoothed is closer than raw
        if (duty > MIN_SPEED_TO_SMOOTH && smoothed < duty) {
            next_duty = smoothed;
        }
    }

    return next_duty;
}

uint8_t fan_points_are_valid(uint8_t count, struct FanPoint *points) __reentrant {
    int i;
    /*
 * Fan curve speeds have to be non-decreasing.
 * Fan curve temperatures have to be increasing.
 */
    for (i = 1; i < count; i++) {
        if (points[i].temp <= points[i - 1].temp || points[i].duty < points[i - 1].duty)
            return false;
    }
    return true;
}
