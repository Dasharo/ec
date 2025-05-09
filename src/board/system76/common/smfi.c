// SPDX-License-Identifier: GPL-3.0-only
//
// This defines a protocol for clients on the AP (application processor) to
// communicate with the EC. The protocol is polled, and uses semaphores to
// ensure correct sequencing.
//
// The client should check that the SMFI_CMD_CMD register is set to CMD_NONE,
// indicating that the EC is waiting for a command. The client should set the
// SMFI_CMD_DATA region as necessary for the command they wish to issue. They
// should then set the SMFI_CMD_CMD byte to indicate to the EC what command to
// execute. The EC will execute the command, and then set SMFI_CMD_RES to the
// correct result. It will finally set SMFI_CMD_CMD to CMD_NONE, to indicate
// the command is complete and the result is available. The client should only
// read the SMFI_CMD_RES value when SMFI_CMD_CMD is set to CMD_NONE.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if !defined(__SCRATCH__)
#include <board/scratch.h>
#include <board/kbled.h>
#include <board/kbscan.h>
#include <board/peci.h>
#include <board/dgpu.h>
#include <board/fan.h>
#include <board/wireless.h>
#include <common/debug.h>

#if CONFIG_SECURITY
#include <board/security.h>
#endif // CONFIG_SECURITY

#endif // !defined(__SCRATCH__)
#include <board/gpio.h>
#include <board/smfi.h>
#include <common/command.h>
#include <common/macro.h>
#include <common/version.h>
#include <board/options.h>
#include <board/kbscan.h>
#include <ec/etwd.h>
#include <ec/pwm.h>

// Shared memory host semaphore
volatile uint8_t __xdata __at(0x1022) SMHSR;
// Host RAM window control
volatile uint8_t __xdata __at(0x105A) HRAMWC;
// Host RAM window 0 base address
volatile uint8_t __xdata __at(0x105B) HRAMW0BA;
// Host RAM window 1 base address
volatile uint8_t __xdata __at(0x105C) HRAMW1BA;
// Host RAM window 0 access allow size
volatile uint8_t __xdata __at(0x105D) HRAMW0AAS;
// Host RAM window 1 access allow size
volatile uint8_t __xdata __at(0x105E) HRAMW1AAS;
// Flash control register 3
volatile uint8_t __xdata __at(0x1063) FLHCTRL3;
// Host RAM window 2 base address
volatile uint8_t __xdata __at(0x1076) HRAMW2BA;
// Host RAM window 3 base address
volatile uint8_t __xdata __at(0x1077) HRAMW3BA;
// Host RAM window 2 access allow size
volatile uint8_t __xdata __at(0x1078) HRAMW2AAS;
// Host RAM window 3 access allow size
volatile uint8_t __xdata __at(0x1079) HRAMW3AAS;

// EC indirect flash access
volatile uint8_t __xdata __at(0x103B) ECINDAR0;
volatile uint8_t __xdata __at(0x103C) ECINDAR1;
volatile uint8_t __xdata __at(0x103D) ECINDAR2;
volatile uint8_t __xdata __at(0x103E) ECINDAR3;
volatile uint8_t __xdata __at(0x103F) ECINDDR;

// Command region - allows client to send commands to EC
#define SMFI_CMD_CMD 0x00
#define SMFI_CMD_RES 0x01
#define SMFI_CMD_DATA 0x02
static volatile uint8_t __xdata __at(0xE00) smfi_cmd[256];

// Debug region - ring buffer of EC firmware prints
#define SMFI_DBG_TAIL 0x00
static volatile uint8_t __xdata __at(0xF00) smfi_dbg[256];

#if !defined(__SCRATCH__)
void smfi_init(void) {
    int16_t i;

    // Clear command region
    for (i = (SMFI_CMD_CMD + 1); i < ARRAY_SIZE(smfi_cmd); i++) {
        smfi_cmd[i] = 0x00;
    }
    // Clear host command last
    smfi_cmd[SMFI_CMD_CMD] = 0x00;

    // Clear debug region
    for (i = (SMFI_DBG_TAIL + 1); i < ARRAY_SIZE(smfi_dbg); i++) {
        smfi_dbg[i] = 0x00;
    }
    // Clear tail last
    smfi_dbg[SMFI_DBG_TAIL] = 0x00;

    // H2RAM window 0 address 0xE00 - 0xEFF, read/write
    HRAMW0BA = 0xE0;
    HRAMW0AAS = 0x04;

    // H2RAM window 1 address 0xF00 - 0xFFF, read-only
    HRAMW1BA = 0xF0;
    HRAMW1AAS = 0x34;

    // Enable H2RAM window 0 and 1 using LPC I/O
    HRAMWC |= BIT(4) | BIT(1) | BIT(0);

    // Enable backup ROM access
    FLHCTRL3 |= BIT(3);
}

static enum Result cmd_print(void) {
    uint8_t flags = smfi_cmd[SMFI_CMD_DATA];
    uint8_t len = smfi_cmd[SMFI_CMD_DATA + 1];

    uint8_t i;
    for (i = 0; (i < len) && ((i + SMFI_CMD_DATA + 2) < ARRAY_SIZE(smfi_cmd)); i++) {
        putchar(smfi_cmd[i + SMFI_CMD_DATA + 2]);
    }

    smfi_cmd[SMFI_CMD_DATA + 1] = i;

    return RES_OK;
}

static enum Result cmd_fan_get(void) {
    switch (smfi_cmd[SMFI_CMD_DATA]) {
    case 0:
        // Get duty of fan 0
        smfi_cmd[SMFI_CMD_DATA + 1] = PWM_REG(CPU_FAN1);
        // TODO handle dual CPU fans
        return RES_OK;
    case 1:
        // Get duty of fan 1
        //TODO: only allow on platforms like addw2
        smfi_cmd[SMFI_CMD_DATA + 1] = PWM_REG(GPU_FAN1);
        return RES_OK;
    }

    // Failed if fan not found
    return RES_ERR;
}

static enum Result cmd_fan_set(void) {
    switch (smfi_cmd[SMFI_CMD_DATA]) {
    case 0:
        // Set duty cycle of fan 0
        PWM_REG(CPU_FAN1) = smfi_cmd[SMFI_CMD_DATA + 1];
#ifdef CPU_FAN2
        // TODO handle CPU fan 2 separately
        PWM_REG(CPU_FAN2) = smfi_cmd[SMFI_CMD_DATA + 1];
#endif
        return RES_OK;
    case 1:
        // Set duty cycle of fan 1
        //TODO: only allow on platforms like addw2
        PWM_REG(GPU_FAN1) = smfi_cmd[SMFI_CMD_DATA + 1];
        return RES_OK;
    }

    // Failed if fan not found
    return RES_ERR;
}

static enum Result cmd_keymap_get(void) {
    int16_t layer = smfi_cmd[SMFI_CMD_DATA];
    int16_t output = smfi_cmd[SMFI_CMD_DATA + 1];
    int16_t input = smfi_cmd[SMFI_CMD_DATA + 2];
    uint16_t key = 0;
    if (keymap_get(layer, output, input, &key)) {
        smfi_cmd[SMFI_CMD_DATA + 3] = (uint8_t)key;
        smfi_cmd[SMFI_CMD_DATA + 4] = (uint8_t)(key >> 8);
        return RES_OK;
    } else {
        return RES_ERR;
    }
}

static enum Result cmd_keymap_set(void) {
    int16_t layer = smfi_cmd[SMFI_CMD_DATA];
    int16_t output = smfi_cmd[SMFI_CMD_DATA + 1];
    int16_t input = smfi_cmd[SMFI_CMD_DATA + 2];
    uint16_t key = ((uint16_t)smfi_cmd[SMFI_CMD_DATA + 3]) |
        (((uint16_t)smfi_cmd[SMFI_CMD_DATA + 4]) << 8);
    //TODO: consider only setting if the key has changed
    if (keymap_set(layer, output, input, key)) {
        //TODO: should we write config on every change?
        if (keymap_save_config()) {
            return RES_OK;
        } else {
            //TODO: need a different error code?
            return RES_ERR;
        }
    } else {
        return RES_ERR;
    }
}

static enum Result cmd_matrix_get(void) {
    smfi_cmd[SMFI_CMD_DATA] = KM_OUT;
    smfi_cmd[SMFI_CMD_DATA + 1] = KM_IN;
    for (uint8_t row = 0; row < KM_OUT; row++) {
        if ((SMFI_CMD_DATA + 2 + row) < ARRAY_SIZE(smfi_cmd)) {
            smfi_cmd[SMFI_CMD_DATA + 2 + row] = kbscan_matrix[row];
        }
    }
    return RES_OK;
}

#if CONFIG_SECURITY
static enum Result cmd_security_get(void) {
    smfi_cmd[SMFI_CMD_DATA] = security_get();
    return RES_OK;
}

static enum Result cmd_security_set(void) {
    enum SecurityState state = smfi_cmd[SMFI_CMD_DATA];
    if (security_set(state)) {
        return RES_OK;
    } else {
        return RES_ERR;
    }
}
#endif // CONFIG_SECURITY

// Command structure: [fan] [temp0] [duty0] ... [temp3] [duty3]
static enum Result cmd_fan_curve_set(void) {
    struct FanPoint points[4];

    for (int i = 0; i < 4; ++i) {
        points[i].temp = smfi_cmd[2 * i + SMFI_CMD_DATA + 1];
        points[i].duty = smfi_cmd[2 * i + SMFI_CMD_DATA + 2] * 255 / 100;
    }

    if (!fan_points_are_valid(4, points))
        return RES_ERR;

    switch (smfi_cmd[SMFI_CMD_DATA]) {
    case 0:
        peci_set_fan_curve(4, points);
        break;
#if HAVE_DGPU
    case 1:
        dgpu_set_fan_curve(4, points);
        break;
#endif
    default:
        return RES_ERR;
    }
    return RES_OK;
}

static enum Result cmd_camera_enablement_set(void) {
    camera_switch_enabled = smfi_cmd[SMFI_CMD_DATA];
    gpio_set(&CCD_EN, smfi_cmd[SMFI_CMD_DATA]);

    return RES_OK;
}

static enum Result cmd_wifi_bt_enablement_set(void) {
    wireless_power(smfi_cmd[SMFI_CMD_DATA]);
    TRACE("WIRELESS %sABLED\n", smfi_cmd[SMFI_CMD_DATA] ? "EN" : "DIS");
    return RES_OK;
}

static enum Result cmd_option_get(void) {
    smfi_cmd[SMFI_CMD_DATA] = options_get(smfi_cmd[SMFI_CMD_DATA]);
    return RES_OK;
}

static enum Result cmd_option_set(void) {
    return !options_set(smfi_cmd[SMFI_CMD_DATA], smfi_cmd[SMFI_CMD_DATA + 1]);
}

#endif // !defined(__SCRATCH__)

#if defined(__SCRATCH__)
static enum Result cmd_spi_scratch(void) __critical {
    uint8_t flags = smfi_cmd[SMFI_CMD_DATA];
    uint8_t len = smfi_cmd[SMFI_CMD_DATA + 1];

    // Enable chip
    if (flags & CMD_SPI_FLAG_BACKUP) {
        ECINDAR3 = 0xFF;
    } else {
        ECINDAR3 = 0x7F;
    }
    ECINDAR2 = 0xFF;
    ECINDAR1 = 0xFD;
    ECINDAR0 = 0x00;

    // Read or write len bytes
    uint8_t i;
    for (i = 0; (i < len) && ((i + SMFI_CMD_DATA + 2) < ARRAY_SIZE(smfi_cmd)); i++) {
        if (flags & CMD_SPI_FLAG_READ) {
            smfi_cmd[i + SMFI_CMD_DATA + 2] = ECINDDR;
        } else {
            ECINDDR = smfi_cmd[i + SMFI_CMD_DATA + 2];
        }
    }

    // Set actually read/written count
    smfi_cmd[SMFI_CMD_DATA + 1] = i;

    if (flags & CMD_SPI_FLAG_DISABLE) {
        // Disable chip
        ECINDAR1 = 0xFE;
        ECINDDR = 0;
    }

    return RES_OK;
}
#endif // defined(__SCRATCH__)

static enum Result cmd_spi(void) {
#if defined(__SCRATCH__)
    return cmd_spi_scratch();
#else // defined(__SCRATCH__)

#if CONFIG_SECURITY
    if (security_get() != SECURITY_STATE_UNLOCK) {
        // EC must be unlocked to allow flashing
        return RES_ERR;
    }
#endif // CONFIG_SECURITY

    if (smfi_cmd[SMFI_CMD_DATA] & CMD_SPI_FLAG_SCRATCH) {
        scratch_trampoline();
    }

    // Cannot use follow mode unless running from scratch rom
    return RES_ERR;
#endif // defined(__SCRATCH__)
}

static enum Result cmd_reset(void) {
#if !defined(__SCRATCH__)

#if CONFIG_SECURITY
    if (security_get() != SECURITY_STATE_UNLOCK) {
        // EC must be unlocked to allow watchdog reset
        return RES_ERR;
    }
#endif // CONFIG_SECURITY

#endif // !defined(__SCRATCH__)

    // Attempt to trigger watchdog reset
    ETWCFG |= BIT(5);
    EWDKEYR = 0;

    // Failed if it got this far
    return RES_ERR;
}

// Set a watchdog timer of 10 seconds
void smfi_watchdog(void) {
    ET1CNTLLR = 0xFF;
    EWDCNTLLR = 0xFF;
    EWDCNTLHR = 0x04;
}

void smfi_event(void) {
    if (smfi_cmd[SMFI_CMD_CMD]) {
#if defined(__SCRATCH__)
        // If in scratch ROM, restart watchdog timer when command received
        smfi_watchdog();
#endif

        switch (smfi_cmd[SMFI_CMD_CMD]) {
#if !defined(__SCRATCH__)
        case CMD_PROBE:
            // Signature
            smfi_cmd[SMFI_CMD_DATA + 0] = 0x76;
            smfi_cmd[SMFI_CMD_DATA + 1] = 0xEC;
            // Version
            smfi_cmd[SMFI_CMD_DATA + 2] = 0x01;
            //TODO: bitmask of implemented commands?
            // Always successful
            smfi_cmd[SMFI_CMD_RES] = RES_OK;
            break;
        case CMD_BOARD:
            strncpy(&smfi_cmd[SMFI_CMD_DATA], board(), ARRAY_SIZE(smfi_cmd) - SMFI_CMD_DATA);
            // Always successful
            smfi_cmd[SMFI_CMD_RES] = RES_OK;
            break;
        case CMD_VERSION:
            strncpy(&smfi_cmd[SMFI_CMD_DATA], version(), ARRAY_SIZE(smfi_cmd) - SMFI_CMD_DATA);
            // Always successful
            smfi_cmd[SMFI_CMD_RES] = RES_OK;
            break;
        case CMD_PRINT:
            smfi_cmd[SMFI_CMD_RES] = cmd_print();
            break;
        case CMD_FAN_GET:
            smfi_cmd[SMFI_CMD_RES] = cmd_fan_get();
            break;
        case CMD_FAN_SET:
            smfi_cmd[SMFI_CMD_RES] = cmd_fan_set();
            break;
        case CMD_KEYMAP_GET:
            smfi_cmd[SMFI_CMD_RES] = cmd_keymap_get();
            break;
        case CMD_KEYMAP_SET:
            smfi_cmd[SMFI_CMD_RES] = cmd_keymap_set();
            break;
        case CMD_MATRIX_GET:
            smfi_cmd[SMFI_CMD_RES] = cmd_matrix_get();
            break;
        case CMD_FAN_CURVE_SET:
            smfi_cmd[SMFI_CMD_RES] = cmd_fan_curve_set();
            break;
        case CMD_CAMERA_ENABLEMENT_SET:
            smfi_cmd[SMFI_CMD_RES] = cmd_camera_enablement_set();
            break;
        case CMD_WIFI_BT_ENABLEMENT_SET:
            smfi_cmd[SMFI_CMD_RES] = cmd_wifi_bt_enablement_set();
            break;
        case CMD_OPTION_GET:
            smfi_cmd[SMFI_CMD_RES] = cmd_option_get();
            break;
        case CMD_OPTION_SET:
            smfi_cmd[SMFI_CMD_RES] = cmd_option_set();
            break;
#if CONFIG_SECURITY
        case CMD_SECURITY_GET:
            smfi_cmd[SMFI_CMD_RES] = cmd_security_get();
            break;
        case CMD_SECURITY_SET:
            smfi_cmd[SMFI_CMD_RES] = cmd_security_set();
            break;
#endif // CONFIG_SECURITY

#endif // !defined(__SCRATCH__)
        case CMD_SPI:
            smfi_cmd[SMFI_CMD_RES] = cmd_spi();
            break;
        case CMD_RESET:
            smfi_cmd[SMFI_CMD_RES] = cmd_reset();
            break;
        default:
            // Command not found
            smfi_cmd[SMFI_CMD_RES] = RES_ERR;
            break;
        }

        // Mark command as finished
        smfi_cmd[SMFI_CMD_CMD] = CMD_NONE;
    }
}

void smfi_debug(uint8_t byte) {
    int16_t tail = (int16_t)smfi_dbg[SMFI_DBG_TAIL];
    tail++;
    if (tail >= ARRAY_SIZE(smfi_dbg)) {
        tail = SMFI_DBG_TAIL + 1;
    }
    smfi_dbg[tail] = byte;
    smfi_dbg[SMFI_DBG_TAIL] = (uint8_t)tail;
}
