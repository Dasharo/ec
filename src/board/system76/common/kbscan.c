// SPDX-License-Identifier: GPL-3.0-only

#include <arch/delay.h>
#include <arch/time.h>
#include <board/acpi.h>
#include <board/fan.h>
#include <board/gpio.h>
#include <board/keymap.h>
#include <board/kbc.h>
#include <board/kbled.h>
#include <board/kbscan.h>
#include <board/lid.h>
#include <board/pmc.h>
#include <board/power.h>
#include <common/macro.h>
#include <common/debug.h>

// Default to not n-key rollover
#ifndef KM_NKEY
#define KM_NKEY 0
#endif // KM_NKEY

bool kbscan_fn_held = false;
bool kbscan_esc_held = false;

bool kbscan_enabled = false;
uint16_t kbscan_repeat_period = 91;
uint16_t kbscan_repeat_delay = 500;

// Raw scan matrix filled by INT84 ISR from hardware KSM result registers.
// 1 = key pressed (active high, ISR inverts the active-low hardware data).
volatile uint8_t kbscan_matrix[KM_OUT] = { 0 };

// Set by ISR when a new scan result is available
volatile bool kbscan_irq_pending = false;

// Set by kbscan_event when a key is held and repeat is pending.
// Allows main loop to keep calling kbscan_event() via timer_0 wakeup
// even when INT84 doesn't fire (hardware KSM only fires on scan change).
volatile bool kbscan_repeat_active = false;

uint8_t sci_extra = 0;

bool camera_switch_enabled = true;

static inline bool matrix_position_is_esc(uint8_t row, uint8_t col) {
    return (row == MATRIX_ESC_OUTPUT) && (col == MATRIX_ESC_INPUT);
}

static inline bool matrix_position_is_fn(uint8_t row, uint8_t col) {
    return (row == MATRIX_FN_OUTPUT) && (col == MATRIX_FN_INPUT);
}

void kbscan_init(void) {
    // Configure KSO: pull-up enabled (bit2), open-drain (bit0)
    KSOCTRL = BIT(2) | BIT(0);
    // Configure KSI: pull-up enabled (bit2)
    KSICTRLR = BIT(2);

    // Put KSO lines in KBM (hardware scan) mode, not GPIO mode
    KSOLGCTRL = 0;   // KSO0-7: hardware scan controller
    KSOHGCTRL = 0;   // KSO8-15: hardware scan controller
    // KSO16/17 (KSOH2) have no GCTRL; driven by hardware when SDEN=1

    // Set all KSI lines to KBS mode (inputs driven by keyboard)
    KSIGCTRL = 0;
    KSIGOEN = 0;
    KSIGDAT = 0;

    // Scan Data Control 2:
    //   WKSOHDLY=0 (23us KSO high delay)
    //   KSOPCS: number of KSO columns
#if KM_OUT <= 16
    SDC2R = 0x00;   // KSOPCS=00: 16 KSO columns
#elif KM_OUT == 17
    SDC2R = 0x40;   // KSOPCS=01: 17 KSO columns
#else
    SDC2R = 0x80;   // KSOPCS=10: 18 KSO columns
#endif

    // Scan Data Control 3:
    //   WKSOLDLY=0 (11us KSO low delay), SDLYBR=1 (1ms between scan rounds)
    SDC3R = 0x01;

    // Scan Data Control 1:
    //   SDEN=1 (enable hardware scan), INTSDVEN=1 (INT84 on scan valid), SLS=001 (2 rounds)
    SDC1R = BIT(7) | BIT(5) | 0x01;

    // Enable INT84 (scan data valid interrupt) in intc_init()
    // (called after kbscan_init from init(), intc_init runs last)
}

// Debounce time in milliseconds
#define DEBOUNCE_DELAY 15

#if KM_NKEY
static bool kbscan_has_ghost_in_row(uint8_t row, uint8_t rowdata) {
    // Use arguments
    row = row;
    rowdata = rowdata;
    return false;
}
#else // KM_NKEY
static inline bool popcount_more_than_one(uint8_t rowdata) {
    return rowdata & (rowdata - 1);
}

static uint8_t kbscan_get_real_keys(uint8_t row, uint8_t rowdata) {
    // Remove any "active" blanks from the matrix.
    uint8_t realdata = 0;
    for (uint8_t col = 0; col < KM_IN; col++) {
        // This tests the default keymap intentionally, to avoid blanks in the
        // dynamic keymap
        if (KEYMAP[0][row][col] && (rowdata & BIT(col))) {
            realdata |= BIT(col);
        }
    }

    return realdata;
}

static bool kbscan_has_ghost_in_row(uint8_t row, uint8_t rowdata) {
    rowdata = kbscan_get_real_keys(row, rowdata);

    // No ghosts exist when less than 2 keys in the row are active.
    if (!popcount_more_than_one(rowdata)) {
        return false;
    }

    // Check against other rows to see if more than one column matches.
    for (uint8_t i = 0; i < KM_OUT; i++) {
        // Read raw scan data for row i from hardware KSM result
        uint8_t otherrow = kbscan_get_real_keys(i, kbscan_matrix[i]);
        if (i != row && popcount_more_than_one(otherrow & rowdata)) {
            return true;
        }
    }

    return false;
}
#endif // KM_NKEY

static void hardware_hotkey(uint16_t key) {
    switch (key) {
    case K_DISPLAY_TOGGLE:
        gpio_set(&BKL_EN, !gpio_get(&BKL_EN));
        break;
    case K_CAMERA_TOGGLE:
        if (camera_switch_enabled)
            gpio_set(&CCD_EN, !gpio_get(&CCD_EN));
        break;
    case K_FAN_TOGGLE:
        fan_max = !fan_max;
        break;
    case K_KBD_BKL:
        kbled_hotkey_step();
        break;
    case K_KBD_COLOR:
        kbled_hotkey_color();
        break;
    case K_KBD_DOWN:
        kbled_hotkey_down();
        break;
    case K_KBD_UP:
        kbled_hotkey_up();
        break;
    case K_KBD_TOGGLE:
        kbled_hotkey_toggle();
        break;
    }
}

bool kbscan_press(uint16_t key, bool pressed, uint8_t *layer) {
    // Wake from sleep on keypress
    if (pressed && lid_state && (power_state == POWER_STATE_S3)) {
        pmc_swi();
    }

    if (key == K_FNLOCK && pressed) {
        DEBUG("Toggling FnLock\n");
        keymap_fnlock ^= 1;
        return true;
    }

    switch (key & KT_MASK) {
    case (KT_NORMAL):
        if (kbscan_enabled) {
            kbc_scancode(key, pressed);
        }
        break;
    case (KT_FN):
        if (layer != NULL) {
            if (pressed)
                *layer = 1;
            else
                *layer = 0;
        } else {
            // In the case no layer can be set, reset bit
            return false;
        }
        break;
    case (KT_COMBO):
        switch (key & 0xFF) {
        case COMBO_DISPLAY_MODE:
            if (kbscan_enabled) {
                if (pressed) {
                    kbc_scancode(K_LEFT_SUPER, true);
                    kbc_scancode(K_P, true);
                    kbc_scancode(K_P, false);
                } else {
                    kbc_scancode(K_LEFT_SUPER, false);
                }
            }
            break;
        case COMBO_PRINT_SCREEN:
            if (kbscan_enabled) {
                if (pressed) {
                    kbc_scancode(KF_E0 | 0x12, true);
                    kbc_scancode(KF_E0 | 0x7C, true);
                } else {
                    kbc_scancode(KF_E0 | 0x7C, false);
                    kbc_scancode(KF_E0 | 0x12, false);
                }
            }
            break;
        case COMBO_PAUSE:
            if (kbscan_enabled) {
                if (pressed) {
                    kbc_scancode(0xE1, true);
                    kbc_scancode(0x14, true);
                    kbc_scancode(0x77, true);
                    kbc_scancode(0xE1, true);
                    kbc_scancode(0x14, false);
                    kbc_scancode(0x77, false);
                }
            }
            break;
        case COMBO_TOUCHPAD:
            // Not actually a combo, just sends a keypress and an SCI
            if (kbscan_enabled)
                kbc_scancode(KF_E0 | 0x63, pressed);

            if (pressed && acpi_ecos != EC_OS_NONE) {
                if (!pmc_sci(&PMC_1, 0x0A)) {
                    // In the case of ignored SCI, reset bit
                    return false;
                }
            }
            break;
        }
        break;
    case (KT_SCI):
        if (pressed) {
            // Send SCI if ACPI OS is loaded
            if (acpi_ecos != EC_OS_NONE) {
                uint8_t sci = (uint8_t)(key & 0xFF);
                if (!pmc_sci(&PMC_1, sci)) {
                    // In the case of ignored SCI, reset bit
                    return false;
                }
            }

            // Handle hardware hotkeys
            hardware_hotkey(key);
        }
        break;
    case (KT_SCI_EXTRA):
        if (pressed) {
            // Send SCI if ACPI OS is loaded
            if (acpi_ecos != EC_OS_NONE) {
                uint8_t sci = SCI_EXTRA;
                sci_extra = (uint8_t)(key & 0xFF);
                if (!pmc_sci(&PMC_1, sci)) {
                    // In the case of ignored SCI, reset bit
                    return false;
                }
            }

            // Handle hardware hotkeys
            hardware_hotkey(key);
        }
        break;
    }
    return true;
}

static inline bool key_should_repeat(uint16_t key) {
    switch (key) {
    case K_AIRPLANE_MODE:
    case K_CAMERA_TOGGLE:
    case K_DISPLAY_TOGGLE:
    case K_FAN_TOGGLE:
    case K_KBD_BKL:
    case K_KBD_COLOR:
    case K_KBD_TOGGLE:
    case K_MIC_MUTE:
    case K_PAUSE:
    case K_SUSPEND:
    case K_TOUCHPAD:
    case K_FNLOCK:
        return false;
    }

    return true;
}

void kbscan_event(void) {
    static uint8_t kbscan_layer = 0;
    uint8_t layer = kbscan_layer;
    static uint8_t kbscan_last_layer[KM_OUT][KM_IN] = { { 0 } };
    static bool kbscan_ghost[KM_OUT] = { false };

    // Debounced state: updated here, compared against kbscan_matrix (raw)
    static uint8_t kbscan_debounced[KM_OUT] = { 0 };

    // Per-row debounce: rows are independent so fast multi-key typing is not blocked
    static bool debounce[KM_OUT] = { false };
    static uint32_t debounce_time[KM_OUT] = { 0 };

    static bool repeat = false;
    static uint16_t repeat_key = 0;
    static uint32_t repeat_key_time = 0;
    static uint32_t repeat_start = 0;

    // Report all keys as released when lid is closed
    if (!lid_state) {
        for (uint8_t i = 0; i < KM_OUT; i++) {
            kbscan_debounced[i] = 0;
        }
        return;
    }

    for (uint8_t i = 0; i < KM_OUT; i++) {
        // Expire per-row debounce (inline time_get to keep DSEG usage minimal)
        if (debounce[i] && (time_get() - debounce_time[i]) >= DEBOUNCE_DELAY)
            debounce[i] = false;

        uint8_t new = kbscan_matrix[i];  // raw scan from ISR (1=pressed)
        uint8_t last = kbscan_debounced[i];
        if (new != last) {
            if (kbscan_has_ghost_in_row(i, new)) {
                kbscan_ghost[i] = true;
                continue;
            } else if (kbscan_ghost[i]) {
                kbscan_ghost[i] = false;
                // Debounce to allow remaining ghosts to settle.
                debounce[i] = true;
                debounce_time[i] = time_get();
            }

            // A key was pressed or released
            for (uint8_t j = 0; j < KM_IN; j++) {
                // clang-format off
                bool new_b = new & BIT(j);
                bool last_b = last & BIT(j);
                // clang-format on
                if (new_b != last_b) {
                    bool reset = false;

                    // If debouncing this row
                    if (debounce[i]) {
                        // Debounce presses and releases
                        reset = true;
                    } else {
                        // Begin debounce for this row
                        debounce[i] = true;
                        debounce_time[i] = time_get();

                        // Check keys used for config reset
                        if (matrix_position_is_esc(i, j))
                            kbscan_esc_held = new_b;
                        if (matrix_position_is_fn(i, j))
                            kbscan_fn_held = new_b;

                        // Handle key press/release
                        if (new_b) {
                            // On a press, cache the layer the key was pressed on
                            kbscan_last_layer[i][j] = kbscan_layer;
                        }
                        uint8_t key_layer = kbscan_last_layer[i][j];
                        uint16_t key = 0;
                        keymap_get(key_layer, i, j, &key);
                        if (key) {
                            DEBUG("KB %d, %d, %d = 0x%04X, %d\n", i, j, key_layer, key, new_b);
                            if (!kbscan_press(key, new_b, &layer)) {
                                // In the case of ignored key press/release, reset bit
                                reset = true;
                            }

                            if (new_b) {
                                // New key pressed, update last key
                                repeat_key = key;
                                repeat_key_time = time_get();
                                repeat = false;
                            } else if (key == repeat_key) {
                                // Repeat key was released
                                repeat_key = 0;
                                repeat = false;
                            }
                        } else {
                            WARN("KB %d, %d, %d missing\n", i, j, kbscan_layer);
                        }
                    }

                    // Reset bit to last state
                    if (reset) {
                        if (last_b) {
                            new |= BIT(j);
                        } else {
                            new &= ~BIT(j);
                        }
                    }
                }
            }

            kbscan_debounced[i] = new;
        } else if (new && repeat_key != 0 && key_should_repeat(repeat_key)) {
            // A key is being held — check typematic repeat
            uint32_t time = time_get();

            if (!repeat) {
                if (time < repeat_key_time) {
                    // Overflow, reset repeat_key_time
                    repeat_key_time = time;
                } else if ((time - repeat_key_time) >= kbscan_repeat_delay) {
                    // Typematic repeat delay elapsed
                    repeat = true;
                    repeat_start = time;
                }
            }

            if (repeat) {
                if ((time - repeat_start) > kbscan_repeat_period) {
                    kbscan_press(repeat_key, true, &layer);
                    repeat_start = time;
                }
            }
        }
    }

    kbscan_layer = layer;

    // Drive repeat from main loop when INT84 doesn't fire (key stable/held)
    kbscan_repeat_active = (repeat_key != 0);
}
