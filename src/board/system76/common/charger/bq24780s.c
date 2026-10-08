// SPDX-License-Identifier: GPL-3.0-only
//
// TI BQ24780S Smart Charger
// https://www.ti.com/lit/ds/symlink/bq24780s.pdf

#include <board/battery.h>
#include <board/smbus.h>
#include <common/macro.h>
#include <common/debug.h>

// clang-format off

// Registers
#define REG_CHARGE_CURRENT 0x14
#define REG_CHARGE_VOLTAGE 0x15
#define REG_DISCHARGE_CURRENT 0x39
#define REG_INPUT_CURRENT 0x3F
#define REG_CHARGE_OPTION_0 0x12
#define REG_CHARGE_OPTION_1 0x3B
#define REG_CHARGE_OPTION_2 0x38
#define REG_CHARGE_OPTION_3 0x37

// ChargeOption0 flags
// Low Power Mode Enable
#define SBC_EN_LWPWR        ((uint16_t)(BIT(15)))
// Watchdog Timer Adjust
#define SBC_WDTMR_ADJ_175S  ((uint16_t)(0b11 << 13))
// Switching Frequency
#define SBC_PWM_FREQ_800KHZ ((uint16_t)(0b01 << 8))
// IDCHG Amplifier Gain
#define SBC_IDCHC_GAIN      ((uint16_t)(BIT(3)))

// ChargeOption1 flags
// Independent Comparator Deglitch
#define SBC_CMP_DEG_1US     (0b01 << 4)
// PMON Gain
#define SBC_PMON_RATIO      BIT(9)
// Adapter:Battery RSENSE ratio
#define SBC_RSENSE_RATIO_1_1    (0b00 << 12)
#define SBC_RSENSE_RATIO_2_1    (0b01 << 12)
#define SBC_RSENSE_RATIO_1_2    (0b10 << 12)
// Battery depletion threshold
#define SBC_BAT_DEPL_VTH    (0b11 << 14)

// ChargeOption3 flags
// Hybrid Power Boost Mode Enable
#define SBC_EN_BOOST        BIT(2)

// Bits 0-5 are ignored. Bits 13-15 must be 0.
#define CHARGE_CURRENT_MASK 0x1FC0

#if CHARGER_BATTERY_RSENSE == 5
    #define CHARGE_CURRENT(X) (MIN(((X) >> 1), CHARGE_CURRENT_MASK) & CHARGE_CURRENT_MASK)
#elif CHARGER_BATTERY_RSENSE == 10
    #define CHARGE_CURRENT(X) (MIN((X), CHARGE_CURRENT_MASK) & CHARGE_CURRENT_MASK)
#elif CHARGER_BATTERY_RSENSE == 20
    #define CHARGE_CURRENT(X) (MIN(((X) << 1), CHARGE_CURRENT_MASK) & CHARGE_CURRENT_MASK)
#else
    #error Invalid battery RSENSE value
#endif

// Bits 0-3, 15 are ignored.
#define CHARGE_VOLTAGE_MASK 0x7FF0
#define CHARGE_VOLTAGE(X) (MIN((X), CHARGE_VOLTAGE_MASK) & CHARGE_VOLTAGE_MASK)

// Bits 0-6 are ignored. Bits 13-15 must be 0.
#define INPUT_CURRENT_MASK 0x1F80

#if CHARGER_ADAPTER_RSENSE == 5
    #define INPUT_CURRENT(X) (MIN(((X) >> 1), INPUT_CURRENT_MASK) & INPUT_CURRENT_MASK)
#elif CHARGER_ADAPTER_RSENSE == 10
    #define INPUT_CURRENT(X) (MIN((X), INPUT_CURRENT_MASK) & INPUT_CURRENT_MASK)
#elif CHARGER_ADAPTER_RSENSE == 20
    #define INPUT_CURRENT(X) (MIN(((X) << 1), INPUT_CURRENT_MASK) & INPUT_CURRENT_MASK)
#else
    #error Invalid adapter RSENSE value
#endif

#if (CHARGER_ADAPTER_RSENSE == 5 && CHARGER_BATTERY_RSENSE == 5) || \
    (CHARGER_ADAPTER_RSENSE == 10 && CHARGER_BATTERY_RSENSE == 10) || \
    (CHARGER_ADAPTER_RSENSE == 20 && CHARGER_BATTERY_RSENSE == 20)
#define RSENSE_RATIO SBC_RSENSE_RATIO_1_1
#elif (CHARGER_ADAPTER_RSENSE == 10 && CHARGER_BATTERY_RSENSE == 5) || \
    (CHARGER_ADAPTER_RSENSE == 20 && CHARGER_BATTERY_RSENSE == 10)
#define RSENSE_RATIO SBC_RSENSE_RATIO_2_1
#elif (CHARGER_ADAPTER_RSENSE == 5 && CHARGER_BATTERY_RSENSE == 10) || \
    (CHARGER_ADAPTER_RSENSE == 10 && CHARGER_BATTERY_RSENSE == 20)
#define RSENSE_RATIO SBC_RSENSE_RATIO_1_2
#else
    #error Invalid adapter:battery RSENSE ratio
#endif

// clang-format on

// XXX: Assumption: ac_last is initialized high.
static bool charger_enabled = false;

bool battery_charger_is_enabled(void) {
    return charger_enabled;
}

// Last values written to the charge current and voltage registers
static uint16_t charge_current_reg = 0;
static uint16_t charge_voltage_reg = 0;

// Program the charge current and voltage requested by the battery, clamped to
// board limits. Registers are only written when the value changes, unless
// forced.
static int16_t charger_set_charge(bool force) {
    int16_t res = 0;
    uint16_t current = battery_charge_current_ma();
    uint16_t voltage = battery_charge_voltage_mv();

    current = CHARGE_CURRENT(current);
    voltage = CHARGE_VOLTAGE(voltage);

    if (force || current != charge_current_reg) {
        // Set charge current in mA
        res = smbus_write(CHARGER_ADDRESS, REG_CHARGE_CURRENT, current);
        if (res < 0)
            return res;
        charge_current_reg = current;
    }

    if (force || voltage != charge_voltage_reg) {
        // Set charge voltage in mV
        res = smbus_write(CHARGER_ADDRESS, REG_CHARGE_VOLTAGE, voltage);
        if (res < 0)
            return res;
        charge_voltage_reg = voltage;
    }

    return 0;
}

// Last value written to the input current register
static uint16_t input_current_reg = 0;

// Program the adapter input current limit. The register is only written when
// the value changes, unless forced.
static int16_t charger_set_input_current(bool force) {
    int16_t res = 0;
    uint16_t current = battery_charger_input_current_ma;

    current = INPUT_CURRENT(current);

    if (force || current != input_current_reg) {
        // Set input current in mA
        res = smbus_write(CHARGER_ADDRESS, REG_INPUT_CURRENT, current);
        if (res < 0)
            return res;
        input_current_reg = current;
    }

    return 0;
}

int16_t battery_charger_disable(void) {
    int16_t res = 0;

    if (!charger_enabled)
        return 0;

    // Set charge option 0 with 175s watchdog
    res = smbus_write(
        CHARGER_ADDRESS,
        REG_CHARGE_OPTION_0,
        SBC_EN_LWPWR | SBC_WDTMR_ADJ_175S | SBC_PWM_FREQ_800KHZ | SBC_IDCHC_GAIN
    );

    // Disable charge current
    res = smbus_write(CHARGER_ADDRESS, REG_CHARGE_CURRENT, 0);
    if (res < 0)
        return res;
    charge_current_reg = 0;

    // Disable charge voltage
    res = smbus_write(CHARGER_ADDRESS, REG_CHARGE_VOLTAGE, 0);
    if (res < 0)
        return res;
    charge_voltage_reg = 0;

    // Keep the input current limit, so the adapter keeps powering the system
    // instead of the battery supplementing it through Hybrid Power Boost
    res = charger_set_input_current(true);
    if (res < 0)
        return res;

    DEBUG("Charger disabled\n");
    charger_enabled = false;
    return 0;
}

int16_t battery_charger_enable(void) {
    int16_t res = 0;
    uint16_t chargeoption3;

    if (charger_enabled)
        return 0;

    res = battery_charger_disable();
    if (res < 0)
        return res;

    // Set charge current and voltage requested by the battery
    res = charger_set_charge(true);
    if (res < 0)
        return res;

    // Set input current in mA
    res = charger_set_input_current(true);
    if (res < 0)
        return res;

    // Set charge option 0 with watchdog disabled
    res = smbus_write(
        CHARGER_ADDRESS,
        REG_CHARGE_OPTION_0,
        SBC_EN_LWPWR | SBC_PWM_FREQ_800KHZ | SBC_IDCHC_GAIN
    );

    // Set the RSENSE ratio
    res = smbus_write(
        CHARGER_ADDRESS,
        REG_CHARGE_OPTION_1,
        SBC_CMP_DEG_1US | SBC_PMON_RATIO | RSENSE_RATIO | SBC_BAT_DEPL_VTH
    );

    // Preserve current ChargeOption3
    res = smbus_read(CHARGER_ADDRESS, REG_CHARGE_OPTION_3, &chargeoption3);
    if (res < 0)
        return res;

    // Enable Hybrid Power Boost (HPB)
    res = smbus_write(CHARGER_ADDRESS, REG_CHARGE_OPTION_3, chargeoption3 | SBC_EN_BOOST);
    if (res < 0)
        return res;

    DEBUG("Charger enabled\n");
    charger_enabled = true;
    return 0;
}

void battery_charger_event(void) {
    //TODO: watchdog

    // Follow changes in the input current limit, also while not charging
    charger_set_input_current(false);

    // Follow changes in the charge current and voltage requested by the battery
    if (charger_enabled)
        charger_set_charge(false);
}

void battery_debug(void) {
    uint16_t data = 0;
    int16_t res = 0;

#define command(N, A, V) \
    { \
        DEBUG("  " #N ": "); \
        res = smbus_read(A, V, &data); \
        if (res < 0) { \
            DEBUG("ERROR %04X\n", -res); \
        } else { \
            DEBUG("%04X\n", data); \
        } \
    }

    DEBUG("Battery:\n");
    command(Temperature, BATTERY_ADDRESS, 0x08);
    command(Voltage, BATTERY_ADDRESS, 0x09);
    command(Current, BATTERY_ADDRESS, 0x0A);
    command(Charge, BATTERY_ADDRESS, 0x0D);
    command(Status, BATTERY_ADDRESS, 0x16);

    DEBUG("Charger (bq24780s):\n");
    command(ChargeOption0, CHARGER_ADDRESS, REG_CHARGE_OPTION_0);
    command(ChargeOption1, CHARGER_ADDRESS, REG_CHARGE_OPTION_1);
    command(ChargeOption2, CHARGER_ADDRESS, REG_CHARGE_OPTION_2);
    command(ChargeOption3, CHARGER_ADDRESS, REG_CHARGE_OPTION_3);
    command(ChargeCurrent, CHARGER_ADDRESS, REG_CHARGE_CURRENT);
    command(ChargeVoltage, CHARGER_ADDRESS, REG_CHARGE_VOLTAGE);
    command(DishargeCurrent, CHARGER_ADDRESS, REG_DISCHARGE_CURRENT);
    command(InputCurrent, CHARGER_ADDRESS, REG_INPUT_CURRENT);
    command(ProchotOption0, CHARGER_ADDRESS, 0x3C);
    command(ProchotOption1, CHARGER_ADDRESS, 0x3D);
    command(ProchotStatus, CHARGER_ADDRESS, 0x3A);

#undef command
}
