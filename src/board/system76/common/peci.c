// SPDX-License-Identifier: GPL-3.0-only

#include <arch/time.h>
#include <board/espi.h>
#include <board/fan.h>
#include <board/gpio.h>
#include <board/peci.h>
#include <board/power.h>
#include <common/debug.h>
#include <common/macro.h>
#include <ec/gpio.h>
#include <ec/pwm.h>

#ifndef USE_S0IX
#define USE_S0IX 0
#endif

// Fan speed is the lowest requested over HEATUP seconds
#ifndef BOARD_HEATUP
#define BOARD_HEATUP 4
#endif

static uint8_t FAN_HEATUP[BOARD_HEATUP] = { 0 };

// Fan speed is the highest HEATUP speed over COOLDOWN seconds
#ifndef BOARD_COOLDOWN
#define BOARD_COOLDOWN 10
#endif

static uint8_t FAN_COOLDOWN[BOARD_COOLDOWN] = { 0 };

bool peci_on = false;
int16_t peci_temp = 0;
uint8_t t_junction = 100;

#define PECI_TEMP(X) ((int16_t)(X))

// clang-format off
#define FAN_POINT(T, D) { .temp = PECI_TEMP(T), .duty = PWM_DUTY(D) }
// clang-format on

// Fan curve with temperature in degrees C, duty cycle in percent
static struct FanPoint FAN_POINTS[] = {
#ifdef BOARD_FAN_POINTS
    BOARD_FAN_POINTS
#else
    FAN_POINT(0, 25),
    FAN_POINT(65, 30),
    FAN_POINT(75, 35),
    FAN_POINT(100, 100)
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

int16_t peci_set_fan_curve(uint8_t count, struct FanPoint *points) {
    if (count != FAN.points_size) {
        TRACE("PECI: Incorrect number of fan points: %d, expected %d\n", count, FAN.points_size);
        return -1;
    }

    for (int i = 0; i < count; ++i) {
        TRACE("PECI: fan curve t%d: %d, d%d: %d\n", i, points[i].temp, i, points[i].duty);
        FAN.points[i].temp = points[i].temp;
        FAN.points[i].duty = points[i].duty;
    }

    return 0;
}

// Returns true if peci is available
bool peci_available(void) {
    // Ensure power state is up to date
    update_power_state();

    // Power state must be S0 for PECI to be useful
    if (power_state != POWER_STATE_S0)
        return false;

#if CONFIG_BUS_ESPI
    // Currently waiting for host reset, PECI is not available
    if (espi_host_reset)
        return false;

    // If VW_PLTRST_N virtual wire is not VWS_HIGH, PECI is not available
    // This is because the CPU has not yet exited reset
    if (vw_get(&VW_PLTRST_N) != VWS_HIGH)
        return false;

    // If VW_HOST_C10 virtual wire is VWS_HIGH, PECI will wake the CPU
    //TODO: wake CPU every 8 seconds following Intel recommendation?
    return (vw_get(&VW_HOST_C10) != VWS_HIGH);
#else
    // PECI is available if PLTRST# is high
    return gpio_get(&BUF_PLT_RST_N);
#endif // CONFIG_BUS_ESPI
}

#if CONFIG_PECI_OVER_ESPI

// Maximum OOB channel response time in ms
#define PECI_ESPI_TIMEOUT 10

void peci_init(void) {}

// Returns true on success, false on error
bool peci_get_temp(int16_t *data) {
    //TODO: Wait for completion?
    // Clear upstream status
    ESUCTRL0 = ESUCTRL0;
    // Clear OOB status
    ESOCTRL0 = ESOCTRL0;

    // Set upstream cycle type
    ESUCTRL1 = ESUCTRL1_OOB;
    // Set upstream tag / length[11:8]
    ESUCTRL2 = 0;
    // Set upstream length [7:0] (size of PECI data plus 3)
    ESUCTRL3 = 8;

    // Destination address (0x10 is PCH, left shifted by one)
    UDB[0] = 0x10 << 1;
    // Command code (0x01 is PECI)
    UDB[1] = 0x01;
    // Set byte count
    UDB[2] = 5;
    // Set source address (0x0F is EC, left shifted by one, or with 1)
    UDB[3] = (0x0F << 1) | 1;
    // PECI target address (0x30 is default)
    UDB[4] = 0x30;
    // PECI write length
    UDB[5] = 1;
    // PECI read length
    UDB[6] = 2;
    // PECI command (0x01 = GetTemp)
    UDB[7] = 0x01;

    // Set upstream enable
    ESUCTRL0 |= ESUCTRL0_ENABLE;
    // Set upstream go
    ESUCTRL0 |= ESUCTRL0_GO;

    // Wait until upstream done
    uint32_t start = time_get();
    while (!(ESUCTRL0 & ESUCTRL0_DONE)) {
        if ((time_get() - start) >= PECI_ESPI_TIMEOUT) {
            DEBUG("peci_get_temp: upstream timeout\n");
            return false;
        }
    }
    // Clear upstream done status
    ESUCTRL0 = ESUCTRL0_DONE;

    // Wait for response
    //TODO: do this asynchronously to avoid delays?
    start = time_get();
    while (!(ESOCTRL0 & ESOCTRL0_STATUS)) {
        if ((time_get() - start) >= PECI_ESPI_TIMEOUT) {
            DEBUG("peci_get_temp: response timeout\n");
            return false;
        }
    }

    // Read response length
    uint8_t len = ESOCTRL4;
    if (len >= 7) {
        //TODO: verify packet type, handle PECI status

        // Received enough data for temperature
        uint8_t low = PUTOOBDB[5];
        uint8_t high = PUTOOBDB[6];
        *data = (((int16_t)high << 8) | (int16_t)low);
        // Clear PUT_OOB status
        ESOCTRL0 = ESOCTRL0_STATUS;
        return true;
    } else {
        // Did not receive enough data
        DEBUG("peci_get_temp: len %d < 7\n", len);
        // Clear PUT_OOB status
        ESOCTRL0 = ESOCTRL0_STATUS;
        return false;
    }
}

// Returns positive completion code on success, negative completion code or
// negative (0x1000 | status register) on PECI hardware error
int16_t peci_wr_pkg_config(uint8_t index, uint16_t param, uint32_t data) {
    //TODO: Wait for completion?
    // Clear upstream status
    ESUCTRL0 = ESUCTRL0;
    // Clear OOB status
    ESOCTRL0 = ESOCTRL0;

    // Set upstream cycle type
    ESUCTRL1 = ESUCTRL1_OOB;
    // Set upstream tag / length[11:8]
    ESUCTRL2 = 0;
    // Set upstream length [7:0] (size of PECI data plus 3)
    ESUCTRL3 = 16;

    // Destination address (0x10 is PCH, left shifted by one)
    UDB[0] = 0x10 << 1;
    // Command code (0x01 is PECI)
    UDB[1] = 0x01;
    // Set byte count
    UDB[2] = 13;
    // Set source address (0x0F is EC, left shifted by one, or with 1)
    UDB[3] = (0x0F << 1) | 1;
    // PECI target address (0x30 is default)
    UDB[4] = 0x30;
    // PECI write length
    UDB[5] = 10;
    // PECI read length
    UDB[6] = 1;
    // PECI command (0xA5 = WrPkgConfig)
    UDB[7] = 0xA5;

    // Write host ID
    UDB[8] = 0;
    // Write index
    UDB[9] = index;
    // Write param
    UDB[10] = (uint8_t)param;
    UDB[11] = (uint8_t)(param >> 8);
    // Write data
    UDB[12] = (uint8_t)data;
    UDB[13] = (uint8_t)(data >> 8);
    UDB[14] = (uint8_t)(data >> 16);
    UDB[15] = (uint8_t)(data >> 24);

    // Set upstream enable
    ESUCTRL0 |= ESUCTRL0_ENABLE;
    // Set upstream go
    ESUCTRL0 |= ESUCTRL0_GO;

    // Wait until upstream done
    uint32_t start = time_get();
    while (!(ESUCTRL0 & ESUCTRL0_DONE)) {
        DEBUG("peci_wr_pkg_config: wait upstream\n");
        if ((time_get() - start) >= PECI_ESPI_TIMEOUT) {
            DEBUG("peci_wr_pkg_config: upstream timeout\n");
            return false;
        }
    }
    // Clear upstream done status
    ESUCTRL0 = ESUCTRL0_DONE;

    // Wait for response
    //TODO: do this asynchronously to avoid delays?
    start = time_get();
    while (!(ESOCTRL0 & ESOCTRL0_STATUS)) {
        DEBUG("peci_wr_pkg_config: wait response\n");
        if ((time_get() - start) >= PECI_ESPI_TIMEOUT) {
            DEBUG("peci_wr_pkg_config: response timeout\n");
            return false;
        }
    }

    // Read response length
    uint8_t len = ESOCTRL4;
    if (len >= 6) {
        //TODO: verify packet type, handle PECI status

        // Received enough data for status code
        int16_t cc = (int16_t)PUTOOBDB[5];

        // Clear PUT_OOB status
        ESOCTRL0 = ESOCTRL0_STATUS;

        if (cc & 0x80) {
            return -cc;
        } else {
            return cc;
        }
    } else {
        // Did not receive enough data
        DEBUG("peci_wr_pkg_config: len %d < 6\n", len);
        // Clear PUT_OOB status
        ESOCTRL0 = ESOCTRL0_STATUS;
        return -0x1000;
    }
}

#else // CONFIG_PECI_OVER_ESPI

// Legacy PECI implementation; requires a dedicated PECI pin connected to the
// PCH and EC (H_PECI).

void peci_init(void) {
    // Allow PECI pin to be used
    GCR2 |= BIT(4);

    // Set frequency to 1MHz
    HOCTL2R = 0x01;
    // Set VTT to 1V
    PADCTLR = 0x02;
}

// Returns true on success, false on error
bool peci_get_temp(int16_t *data) {
    // Wait for any in-progress transaction to complete
    while (HOSTAR & BIT(0)) {}
    // Clear status
    HOSTAR = HOSTAR;

    // Enable PECI, clearing data fifo's
    HOCTLR = BIT(5) | BIT(3);
    // Set address to default
    HOTRADDR = 0x30;
    // Set write length
    HOWRLR = 1;
    // Set read length
    HORDLR = 2;
    // Set command
    HOCMDR = 1;
    // Start transaction
    HOCTLR |= 1;

    // Wait for command completion
    while (!(HOSTAR & BIT(1))) {}

    uint8_t status = HOSTAR;
    if (status & 0xEC) {
        ERROR("peci_get_temp: hardware error: 0x%02X\n", status);
        // Clear status
        HOSTAR = HOSTAR;
        return false;
    } else {
        // Read two byte temperature data if finished successfully
        uint8_t low = HORDDR;
        uint8_t high = HORDDR;
        *data = (((int16_t)high << 8) | (int16_t)low);

        // Clear status
        HOSTAR = HOSTAR;
        return true;
    }
}

// Returns positive completion code on success, negative completion code or
// negative (0x1000 | status register) on PECI hardware error
int16_t peci_wr_pkg_config(uint8_t index, uint16_t param, uint32_t data) {
    int retry = 50; // TODO how many retries are appropriate?
    uint8_t cc = HORDDR;

    // Wait for any in-progress transaction to complete
    while (HOSTAR & BIT(0)) {}
    do {
        // Clear status
        HOSTAR = HOSTAR;

        // Enable PECI, clearing data fifo's, enable AW_FCS
        HOCTLR = BIT(5) | BIT(3) | BIT(1);
        // Set address to default
        HOTRADDR = 0x30;
        // Set write length
        HOWRLR = 10;
        // Set read length
        HORDLR = 1;
        // Set command
        HOCMDR = 0xA5;

        // Write host ID
        HOWRDR = 0;
        // Write index
        HOWRDR = index;
        // Write param
        HOWRDR = (uint8_t)param;
        HOWRDR = (uint8_t)(param >> 8);
        // Write data
        HOWRDR = (uint8_t)data;
        HOWRDR = (uint8_t)(data >> 8);
        HOWRDR = (uint8_t)(data >> 16);
        HOWRDR = (uint8_t)(data >> 24);

        // Start transaction
        HOCTLR |= 1;

        // Wait for command completion
        while (!(HOSTAR & BIT(1))) {}

        uint8_t status = HOSTAR;
        if (status & 0xEC) {
            ERROR("peci_wr_pkg_config: hardware error: 0x%02X\n", status);
            // Clear status
            HOSTAR = HOSTAR;
            return -(0x1000 | status);
        }

        cc = HORDDR;

        // Clear status
        HOSTAR = HOSTAR;

        if (cc == 0x40) {
            TRACE("peci_wr_pkg_config: command successful\n");
            return cc;
        }

    } while (cc == 0x80 || cc == 0x81 || !retry--);

    ERROR("peci_wr_pkg_config: command error: 0x%02X\n", cc);
    return -((int16_t)cc);
}

int16_t peci_rd_pkg_config(uint8_t index, uint16_t param, uint32_t *value) {
    int retry = 50; // TODO how many retries are appropriate?
    uint8_t cc = HORDDR;
    *value = 0;

    // Wait for any in-progress transaction to complete
    while (HOSTAR & BIT(0)) {}
    do {
        // Clear status
        HOSTAR = HOSTAR;

        // Enable PECI, clearing data fifo's, enable AW_FCS
        HOCTLR = BIT(5) | BIT(3) | BIT(1);
        // Set address to default
        HOTRADDR = 0x30;
        // Set write length
        HOWRLR = 5;
        // Set read length
        HORDLR = 5;

        // Set command
        HOCMDR = 0xA1;
        // Write host ID
        HOWRDR = 0;
        // Write index
        HOWRDR = index;
        // Write param
        HOWRDR = (uint8_t)param;
        HOWRDR = (uint8_t)(param >> 8);

        // Start transaction
        HOCTLR |= 1;

        // Wait for command completion
        while (!(HOSTAR & BIT(1))) {}

        uint8_t status = HOSTAR;
        if (status & 0xEC) {
            ERROR("peci_rd_pkg_config: hardware error: 0x%02X\n", status);
            // Clear status
            HOSTAR = HOSTAR;
            return -(0x1000 | status);
        }

        cc = HORDDR;

        // Clear status
        HOSTAR = HOSTAR;
    } while (cc & 0x80 || !retry--);

    if (cc != 0x40) {
        ERROR("peci_rd_pkg_config: hardware error: 0x%02X\n", cc);
        return -((int16_t)cc);
    } else {
        // Read data if finished successfully
        for (int i = 0; i < 4; ++i) {
            *value |= (((uint32_t)HORDDR) << (8 * i));
        }

        // Clear status
        HOSTAR = HOSTAR;
        return 0;
    }
}

#endif // CONFIG_PECI_OVER_ESPI

// PECI information can be found here: https://www.intel.com/content/dam/www/public/us/en/documents/design-guides/core-i7-lga-2011-guide.pdf
uint8_t peci_get_fan_duty(void) {
    uint8_t duty;

    peci_on = peci_available();
    if (peci_on) {
        int16_t peci_offset = 0;
        if (peci_get_temp(&peci_offset)) {
            // Use result if finished successfully
            peci_temp = PECI_TEMP(t_junction) + (peci_offset >> 6);
            duty = fan_duty(&FAN, peci_temp);
        } else {
            // Default to 50% if there is an error
            peci_temp = 0;
            duty = PWM_DUTY(50);
        }
    } else {
        // Turn fan off if not in S0 state
        peci_temp = 0;
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

    TRACE("PECI temp=%d\n", peci_temp);
    TRACE("fan duty set to=%d\n", duty);
    return duty;
}
