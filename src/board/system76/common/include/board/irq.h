// SPDX-License-Identifier: GPL-3.0-only

#ifndef _BOARD_IRQ_H
#define _BOARD_IRQ_H

#include <stdbool.h>

// Pending flags set by external_1 ISR, cleared by main loop handlers.
extern volatile bool acin_irq_pending;       // ACIN_N (AC adapter)
extern volatile bool pwr_sw_irq_pending;     // PWR_SW_N (power button)
extern volatile bool sys_pwrgd_irq_pending;  // ALL_SYS_PWRGD
extern volatile bool plt_rst_irq_pending;    // BUF_PLT_RST_N
extern volatile bool slp_sus_irq_pending;    // SLP_SUS_N
extern volatile bool lan_wakeup_irq_pending; // LAN_WAKEUP_N
extern volatile bool usbpd_irq_pending;      // JACK_IN_N / SINK_CTRL / PD_IRQ
extern volatile bool lid_irq_pending;
extern volatile bool smfi_irq_pending;
extern volatile bool kbc_irq_pending;
extern volatile bool pmc_irq_pending;
extern volatile bool dgpu_irq_pending;
extern volatile bool espi_irq_pending;

#endif // _BOARD_IRQ_H
