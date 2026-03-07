// SPDX-License-Identifier: GPL-3.0-only

#include <board/usbpd.h>

void usbpd_init(void) {}

void usbpd_event(void) {}

void usbpd_disable_charging(void) {}

int8_t usbpd_ucsi(uint8_t *control, uint8_t *out_data, uint8_t *out_len) {
    (void)control; (void)out_data; (void)out_len;
    return -1;
}
