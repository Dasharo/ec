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

// Charging is inhibited because the battery is too hot
static bool temp_inhibit = false;

/**
 * Whether charging is inhibited due to high battery temperature.
 */
bool battery_charge_temp_inhibited(void) {
    return temp_inhibit;
}

/**
 * Whether the battery reports a usable Temperature() reading.
 */
bool battery_temp_valid(void) {
    return battery_info.temp >= BATTERY_TEMP_VALID_MIN;
}

/**
 * Update the battery temperature charging gate from the latest battery data.
 *
 * Charging is inhibited when Temperature() (SBS 0x08) reaches
 * BATTERY_CHARGE_TEMP_MAX, or when the pack sets OVER_TEMP_ALARM in
 * BatteryStatus() (SBS 0x16). It is allowed again only once the temperature
 * is at or below BATTERY_CHARGE_TEMP_RESUME, the alarm is cleared, and, per
 * Smart Battery Data Specification rev 1.1, the battery requests charge again
 * through non-zero ChargingCurrent() and ChargingVoltage().
 *
 * A reading below BATTERY_TEMP_VALID_MIN is treated as no reading and only the
 * alarm bit is used. Gate state changes are logged at WARN level so they show
 * up in release builds. The EC log is only a small ring buffer read live, so
 * an invalid reading is logged again every BATTERY_TEMP_INVALID_LOG_EVENTS
 * battery events for as long as it persists while a battery is connected.
 */
static void battery_temp_gate_update(void) {
    static bool temp_valid_last = true;
    static uint8_t invalid_events = 0;
    bool temp_valid = battery_temp_valid();
    bool alarm = (battery_info.status & BATTERY_OVER_TEMP_ALARM) != 0;
    // Do not warn when no battery is connected, the reading is then always 0
    bool present = (battery_info.status & BATTERY_INITIALIZED) != 0;

    if (!temp_valid) {
        if (present && (temp_valid_last || ++invalid_events >= BATTERY_TEMP_INVALID_LOG_EVENTS)) {
            WARN("Battery temperature invalid: %d dK, using alarm only\n", battery_info.temp);
            invalid_events = 0;
        }
    } else if (!temp_valid_last) {
        WARN("Battery temperature valid: %d dK\n", battery_info.temp);
    }
    temp_valid_last = temp_valid;

    if (!temp_inhibit) {
        if (alarm || (temp_valid && battery_info.temp >= BATTERY_CHARGE_TEMP_MAX)) {
            WARN(
                "Battery over temperature: %d dK, alarm %d, charging inhibited\n",
                battery_info.temp,
                (int)alarm
            );
            temp_inhibit = true;
        }
    } else {
        bool cooled = !temp_valid || battery_info.temp <= BATTERY_CHARGE_TEMP_RESUME;
        if (!alarm && cooled && battery_requests_charge()) {
            WARN("Battery temperature normal: %d dK, charging allowed\n", battery_info.temp);
            temp_inhibit = false;
        }
    }
}

/**
 * Charge current limit for the current state of charge. Full board limit up to
 * BATTERY_TAPER_START, then dropping linearly to BATTERY_TAPER_MIN_CURRENT at
 * 100%.
 */
static uint16_t battery_taper_current_ma(void) {
#if BATTERY_TAPER_START < 100 && BATTERY_TAPER_MIN_CURRENT < CHARGER_CHARGE_CURRENT
    uint16_t charge = MIN(battery_info.charge, 100);

    if (charge <= BATTERY_TAPER_START)
        return CHARGER_CHARGE_CURRENT;

    return CHARGER_CHARGE_CURRENT -
        (uint16_t)((uint32_t)(CHARGER_CHARGE_CURRENT - BATTERY_TAPER_MIN_CURRENT) *
                   (charge - BATTERY_TAPER_START) / (100 - BATTERY_TAPER_START));
#else
    return CHARGER_CHARGE_CURRENT;
#endif
}

/**
 * Charge current to program, as requested by the battery but never exceeding
 * the board limit, tapered near full charge.
 */
uint16_t battery_charge_current_ma(void) {
    return MIN(battery_info.charging_current, battery_taper_current_ma());
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

    // Never charge a hot battery, regardless of the charging thresholds. The
    // threshold state is kept so charging resumes once the battery cools down.
    if (temp_inhibit)
        return battery_charger_disable();

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

    TRACE("BAT %d mV %d mA %d dK\n", battery_info.voltage, battery_info.current, battery_info.temp);

    battery_temp_gate_update();
    battery_charger_event();
}
