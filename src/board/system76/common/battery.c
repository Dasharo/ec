// SPDX-License-Identifier: GPL-3.0-only

#include <board/battery.h>
#include <board/options.h>
#include <board/smbus.h>
#include <common/debug.h>
#include <common/macro.h>

struct battery_info battery_info = { 0 };

uint16_t battery_charger_input_current_ma = CHARGER_INPUT_CURRENT;
uint16_t battery_charger_input_voltage_v = BATTERY_CHARGER_VOLTAGE_AC;

// Default values to disable battery charging thresholds
#define BATTERY_START_DEFAULT 0
#define BATTERY_END_DEFAULT 100

uint8_t battery_get_start_threshold(void) {
    if (options_get(OPT_BAT_THRESHOLD_START) > 100)
        return BATTERY_START_DEFAULT;
    return options_get(OPT_BAT_THRESHOLD_START);
}

bool battery_set_start_threshold(uint8_t value) {
    if (value > 100 || value >= options_get(OPT_BAT_THRESHOLD_STOP))
        return false;

    options_set(OPT_BAT_THRESHOLD_START, value);
    return true;
}

uint8_t battery_get_end_threshold(void) {
    if (options_get(OPT_BAT_THRESHOLD_STOP) > 100)
        return BATTERY_END_DEFAULT;
    return options_get(OPT_BAT_THRESHOLD_STOP);
}

bool battery_set_end_threshold(uint8_t value) {
    if (value > 100 || value <= options_get(OPT_BAT_THRESHOLD_START))
        return false;

    options_set(OPT_BAT_THRESHOLD_STOP, value);
    return true;
}

/**
 * Whether the battery requests to be charged, per Smart Battery Data
 * Specification rev 1.1 ChargingCurrent() and ChargingVoltage().
 */
bool battery_requests_charge(void) {
    return battery_info.charging_current && battery_info.charging_voltage;
}

/**
 * Charge current to program, as requested by the battery but never exceeding
 * the board limit.
 */
uint16_t battery_charge_current_ma(void) {
    return MIN(battery_info.charging_current, CHARGER_CHARGE_CURRENT);
}

/**
 * Charge voltage to program, as requested by the battery but never exceeding
 * the board limit.
 */
uint16_t battery_charge_voltage_mv(void) {
    return MIN(battery_info.charging_voltage, CHARGER_CHARGE_VOLTAGE);
}

/**
 * Configure the charger based on charging threshold values.
 */
int16_t battery_charger_configure(void) {
    static bool should_charge = false;
    // Charging was stopped because the end threshold was reached
    static bool stopped_at_end = false;
    uint8_t end = battery_get_end_threshold();
    uint8_t start = battery_get_start_threshold();

    if (start == BATTERY_START_DEFAULT) {
        // No start threshold configured: Charge up to the end threshold. Once
        // stopped there, restart charging only after the battery
        // self-discharges below the hysteresis band.
        if (stopped_at_end)
            start = end > BATTERY_RECHARGE_HYSTERESIS ? end - BATTERY_RECHARGE_HYSTERESIS : 0;
        else
            start = end;
    }

    if (battery_info.charge >= end) {
        // Stop threshold configured: Stop charging at threshold.
        should_charge = false;
        stopped_at_end = true;
    } else if (battery_info.charge < start) {
        // Start threshold reached: Start charging only if the battery
        // requests it, per Smart Battery Data Specification rev 1.1
        // ChargingCurrent() and ChargingVoltage().
        if (battery_requests_charge())
            should_charge = true;
    }

    if (should_charge) {
        stopped_at_end = false;
        return battery_charger_enable();
    }
    return battery_charger_disable();
}

void battery_event(void) {
    int16_t res = 0;

#define command(N, V) \
    { \
        res = smbus_read(BATTERY_ADDRESS, V, &N); \
        if (res < 0) { \
            N = 0; \
        } \
    }

    command(battery_info.temp, 0x08);
    command(battery_info.voltage, 0x09);
    command(battery_info.current, 0x0A);
    command(battery_info.charge, 0x0D);
    command(battery_info.remaining_capacity, 0x0F);
    command(battery_info.full_capacity, 0x10);
    command(battery_info.status, 0x16);
    command(battery_info.cycle_count, 0x17);
    command(battery_info.design_capacity, 0x18);
    command(battery_info.design_voltage, 0x19);
    command(battery_info.charging_current, 0x14);
    command(battery_info.charging_voltage, 0x15);

#undef command

    TRACE("BAT %d mV %d mA\n", battery_info.voltage, battery_info.current);

    battery_charger_event();
}
