// SPDX-License-Identifier: GPL-3.0-only

#ifndef _BOARD_BATTERY_H
#define _BOARD_BATTERY_H

#include <common/macro.h>

#include <stdbool.h>
#include <stdint.h>

#ifndef BATTERY_ADDRESS
#define BATTERY_ADDRESS 0x0B
#endif

#ifndef CHARGER_ADDRESS
#define CHARGER_ADDRESS 0x09
#endif

// Smart Battery BatteryStatus() flags
#define BATTERY_FULLY_CHARGED BIT(5)
#define BATTERY_INITIALIZED BIT(7)
#define BATTERY_OVER_TEMP_ALARM BIT(12)

// Discharge current below which a fully charged battery on AC is not
// reported as discharging. Some charger, battery and adapter combinations
// make the battery leak a trivial amount of current, which would otherwise
// cause the OS to rapidly flip between charge states.
#ifndef BATTERY_DISCHARGE_DEADBAND_MA
#define BATTERY_DISCHARGE_DEADBAND_MA 10
#endif

// When no start threshold is configured, resume charging once the battery
// self-discharges this many percent below the end threshold.
#ifndef BATTERY_RECHARGE_HYSTERESIS
#define BATTERY_RECHARGE_HYSTERESIS 5
#endif

// Battery temperature charging gate, in 0.1 K as returned by Smart Battery
// Temperature(). Charging is inhibited at or above BATTERY_CHARGE_TEMP_MAX and
// may resume once the temperature drops to or below BATTERY_CHARGE_TEMP_RESUME.
// The defaults (45 C / 40 C) match the upper bound of the 0-45 C charge range
// typical of Li-ion laptop packs, below the pack BMS's own cut-off, so the EC
// acts first. Boards whose pack has a different rated range should override.
#define BATTERY_TEMP_DK(c) (2732 + (c) * 10)
#ifndef BATTERY_CHARGE_TEMP_MAX
#define BATTERY_CHARGE_TEMP_MAX BATTERY_TEMP_DK(45)
#endif
#ifndef BATTERY_CHARGE_TEMP_RESUME
#define BATTERY_CHARGE_TEMP_RESUME BATTERY_TEMP_DK(40)
#endif
// Temperature() readings below this are treated as no reading, e.g. a failed
// read (0), an open thermistor or a pack without a thermistor wired to the
// gauge. None of these indicate a hot pack, so the gate then falls back to the
// pack's BatteryStatus() OVER_TEMP_ALARM bit. There is no upper bound: any
// reading at or above BATTERY_CHARGE_TEMP_MAX, however implausible (e.g. a
// shorted thermistor), inhibits charging.
#define BATTERY_TEMP_VALID_MIN BATTERY_TEMP_DK(-40)

#if BATTERY_CHARGE_TEMP_RESUME >= BATTERY_CHARGE_TEMP_MAX
#error "BATTERY_CHARGE_TEMP_RESUME must be below BATTERY_CHARGE_TEMP_MAX"
#endif

#ifndef AC_ADAPTER_VOLTAGE
#define AC_ADAPTER_VOLTAGE 19
#endif

#define BATTERY_CHARGER_VOLTAGE_AC AC_ADAPTER_VOLTAGE
#define BATTERY_CHARGER_VOLTAGE_PD 20

struct battery_info {
    uint16_t temp;
    uint16_t voltage;
    uint16_t current;
    uint16_t charge;
    uint16_t remaining_capacity;
    uint16_t full_capacity;
    uint16_t status;
    uint16_t cycle_count;
    uint16_t design_capacity;
    uint16_t design_voltage;
    uint16_t charging_current;
    uint16_t charging_voltage;
};
extern struct battery_info battery_info;

extern uint16_t battery_charger_input_current_ma;
extern uint16_t battery_charger_input_voltage_v;

uint8_t battery_get_start_threshold(void);
bool battery_set_start_threshold(uint8_t value);

uint8_t battery_get_end_threshold(void);
bool battery_set_end_threshold(uint8_t value);

bool battery_requests_charge(void);
bool battery_charge_temp_inhibited(void);
uint16_t battery_charge_current_ma(void);
uint16_t battery_charge_voltage_mv(void);

int16_t battery_charger_configure(void);
void battery_event(void);

// Defined by charger/*.c
int16_t battery_charger_disable(void);
int16_t battery_charger_enable(void);
bool battery_charger_is_enabled(void);
void battery_charger_event(void);
void battery_debug(void);

#endif // _BOARD_BATTERY_H
