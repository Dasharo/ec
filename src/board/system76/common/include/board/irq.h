// SPDX-License-Identifier: GPL-3.0-only

#ifndef _BOARD_IRQ_H
#define _BOARD_IRQ_H

#include <stdbool.h>

// Pending flags set by external_1 ISR, cleared by main loop handlers.
extern volatile bool power_irq_pending;
extern volatile bool lid_irq_pending;
extern volatile bool smfi_irq_pending;
extern volatile bool kbc_irq_pending;
extern volatile bool pmc_irq_pending;
extern volatile bool dgpu_irq_pending;
extern volatile bool espi_irq_pending;

#endif // _BOARD_IRQ_H
