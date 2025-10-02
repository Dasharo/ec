// SPDX-License-Identifier: GPL-3.0-only

#include <board/dgpu.h>
#include <board/fan.h>

#if HAVE_DGPU

#include <board/battery.h>
#include <board/gpio.h>
#include <board/options.h>
#include <board/peci.h>
#include <board/power.h>
#include <common/debug.h>
#include <common/macro.h>
#include <ec/i2c.h>
#include <ec/pwm.h>

// Fan speed is the lowest requested over HEATUP seconds
#ifndef BOARD_DGPU_HEATUP
#define BOARD_DGPU_HEATUP 12
#endif

static uint8_t FAN_HEATUP[BOARD_DGPU_HEATUP] = { 0 };

// Fan speed is the highest HEATUP speed over COOLDOWN seconds
#ifndef BOARD_DGPU_COOLDOWN
#define BOARD_DGPU_COOLDOWN 10
#endif

static uint8_t FAN_COOLDOWN[BOARD_DGPU_COOLDOWN] = { 0 };

int16_t dgpu_temp = 0;

#define DGPU_TEMP(X) ((int16_t)(X))

#define FAN_POINT(T, D) { .temp = DGPU_TEMP(T), .duty = PWM_DUTY(D) }

// Fan curve with temperature in degrees C, duty cycle in percent
static struct FanPoint FAN_POINTS[] = {
#ifdef BOARD_DGPU_FAN_POINTS
    BOARD_DGPU_FAN_POINTS
#else
    FAN_POINT(70, 40),
    FAN_POINT(75, 50),
    FAN_POINT(80, 60),
    FAN_POINT(85, 65),
    FAN_POINT(90, 65)
#endif
};

static struct Fan FAN = {
    .points = FAN_POINTS,
    .points_size = ARRAY_SIZE(FAN_POINTS),
    .heatup = FAN_HEATUP,
    .heatup_size = ARRAY_SIZE(FAN_HEATUP),
    .cooldown = FAN_COOLDOWN,
    .cooldown_size = ARRAY_SIZE(FAN_COOLDOWN),
    .interpolate = SMOOTH_FANS != 0,
};

uint8_t dgpu_get_d_notify_level(bool ac) {
#if HAVE_D_NOTIFY
    uint16_t supply_watts = 0;

    if (ac) {
        supply_watts = (uint32_t)battery_charger_input_current_ma *
            (uint32_t)battery_charger_input_voltage_v / 1000;

        if (supply_watts >= D_NOTIFY_D1_MIN_W)
            return 0;

        if (supply_watts >= D_NOTIFY_D2_MIN_W)
            return 1;

        if (supply_watts >= D_NOTIFY_D3_MIN_W)
            return 2;

        if (supply_watts >= D_NOTIFY_D4_MIN_W)
            return 3;

        if (supply_watts >= D_NOTIFY_D5_MIN_W)
            return 4;

        // If we have less than required for D5, we're in trouble. Do the best
        // we can and set level to D4 anyway.
        // TODO: Throttle the GPU further
        return 4;
    }

#endif
    /* Fix unused variable */
    ac = ac;

    return 0;
}

int16_t dgpu_set_fan_curve(uint8_t count, struct FanPoint *points) {
    if (count != FAN.points_size) {
        TRACE("DGPU: Incorrect number of fan points: %d, expected %d\n", count, FAN.points_size);
        return -1;
    }

    for (int i = 0; i < count; ++i) {
        TRACE("DGPU: fan curve t%d: %d, d%d: %d\n", i, points[i].temp, i, points[i].duty);
        FAN.points[i].temp = points[i].temp;
        FAN.points[i].duty = points[i].duty;
    }

    return 0;
}

void dgpu_init(void) {
    // Set up for i2c usage
    i2c_reset(&I2C_DGPU, true);
}

uint8_t dgpu_get_fan_duty(void) {
    uint8_t duty;
    if (power_state == POWER_STATE_S0 && gpio_get(&DGPU_PWR_EN) && !gpio_get(&GC6_FB_EN)) {
        // Use I2CS if in S0 state
        int8_t rlts;
        int16_t res = i2c_get(&I2C_DGPU, 0x4F, 0x00, &rlts, 1);
        if (res == 1) {
            dgpu_temp = (int16_t)rlts;
            duty = fan_duty(&FAN, dgpu_temp);
        } else {
            DEBUG("DGPU temp error: %d\n", res);
            // Default to 50% if there is an error
            dgpu_temp = 0;
            duty = PWM_DUTY(50);
        }
    } else {
        // Turn fan off if not in S0 state or GPU power not on
        dgpu_temp = 0;
        duty = PWM_DUTY(0);
    }

    if (peci_on && fan_max) {
        // Override duty if fans are manually set to maximum
        duty = PWM_DUTY(100);
    } else {
        // Apply heatup and cooldown filters to duty
        duty = fan_heatup(&FAN, duty);
        duty = fan_cooldown(&FAN, duty);
    }

    TRACE("DGPU temp=%d\n", dgpu_temp);
    return duty;
}

#if HAVE_MUX_CTRL_BIOS
// choose between dgpu and igpu for internal display
void set_mux_ctrl(void) {
    gpio_set(&MUX_CTRL_BIOS, !!options_get(OPT_GPU_MUX_CTRL));
}
#endif // HAVE_MUX_CTRL_BIOS

#else

void dgpu_init(void) {}

uint8_t dgpu_get_fan_duty(void) {
    return PWM_DUTY(0);
}

#endif // HAVE_DGPU
