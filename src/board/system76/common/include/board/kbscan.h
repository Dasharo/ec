// SPDX-License-Identifier: GPL-3.0-only

#ifndef _BOARD_KBSCAN_H
#define _BOARD_KBSCAN_H

#include <stdbool.h>

#include <board/keymap.h>
#include <ec/kbscan.h>

// EC config reset key combo: Fn+Esc
extern bool kbscan_fn_held;
extern bool kbscan_esc_held;

extern bool kbscan_enabled;

extern bool camera_switch_enabled;

// ms between repeating key
extern uint16_t kbscan_repeat_period;
// ms between pressing key and repeating
extern uint16_t kbscan_repeat_delay;

// Debounced kbscan matrix
extern uint8_t kbscan_matrix[KM_OUT];

void kbscan_init(void);
void kbscan_event(void);

#endif // _BOARD_KBSCAN_H
