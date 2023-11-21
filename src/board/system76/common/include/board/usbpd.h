// SPDX-License-Identifier: GPL-3.0-only

#ifndef _BOARD_USBPD_H
#define _BOARD_USBPD_H

enum {
    // Fully functional, normal operation
    USBPD_MODE_APP = 0,
    // PD controller is running BIST
    USBPD_MODE_BIST,
    // PD controller booted in dead battery mode
    USBPD_MODE_BOOT,
    // Simulated port disconnect by previously issued DISC command
    USBPD_MODE_DISC,
    // Other values indicate limited functionality
    USBPD_MODE_UNKNOWN,
};

void usbpd_init(void);
void usbpd_event(void);
void usbpd_disable_charging(void);
void usbpd_enable_charging(void);
void usbpd_set_mode(int16_t mode);

#endif // _BOARD_USBPD_H
