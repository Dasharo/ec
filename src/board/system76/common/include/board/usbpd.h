// SPDX-License-Identifier: GPL-3.0-only

#ifndef _BOARD_USBPD_H
#define _BOARD_USBPD_H

#include <stdint.h>

void usbpd_init(void);
void usbpd_reset(void);
void usbpd_event(void);
int8_t usbpd_ucsi(uint8_t *control, uint8_t *out_data, uint8_t *out_len);

#endif // _BOARD_USBPD_H
