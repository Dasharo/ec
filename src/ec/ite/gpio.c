// SPDX-License-Identifier: GPL-3.0-only

#include <ec/gpio.h>
#include <ec/intc.h>
#include <ec/wuc.h>
#include <common/debug.h>

bool gpio_get(struct Gpio *gpio) {
    if (*(gpio->data) & gpio->value) {
        return true;
    } else {
        return false;
    }
}

void gpio_set(struct Gpio *gpio, bool value) {
    if (value) {
        *(gpio->data) |= gpio->value;
    } else {
        *(gpio->data) &= ~(gpio->value);
    }
}

// Chrome EC-style: return WUEMR register pointer for a WUC group number.
volatile uint8_t __xdata *gpio_wuemr(uint8_t group) {
    switch (group) {
    case  1: return &WUEMR1;
    case  2: return &WUEMR2;
    case  3: return &WUEMR3;
    case  4: return &WUEMR4;
    case  6: return &WUEMR6;
    case  7: return &WUEMR7;
    case  8: return &WUEMR8;
    case  9: return &WUEMR9;
    case 10: return &WUEMR10;
    case 11: return &WUEMR11;
    case 12: return &WUEMR12;
    case 13: return &WUEMR13;
    case 14: return &WUEMR14;
    default: return 0;
    }
}

// Chrome EC-style: return WUESR register pointer for a WUC group number.
volatile uint8_t __xdata *gpio_wuesr(uint8_t group) {
    switch (group) {
    case  1: return &WUESR1;
    case  2: return &WUESR2;
    case  3: return &WUESR3;
    case  4: return &WUESR4;
    case  6: return &WUESR6;
    case  7: return &WUESR7;
    case  8: return &WUESR8;
    case  9: return &WUESR9;
    case 10: return &WUESR10;
    case 11: return &WUESR11;
    case 12: return &WUESR12;
    case 13: return &WUESR13;
    case 14: return &WUESR14;
    default: return 0;
    }
}

// Return IER register pointer for an IRQ number (IER_reg = irq / 8).
volatile uint8_t __xdata *gpio_ier(uint8_t irq) {
    switch (irq >> 3) {
    case  0: return &IER0;
    case  1: return &IER1;
    case  2: return &IER2;
    case  3: return &IER3;
    case  4: return &IER4;
    case  5: return &IER5;
    case  6: return &IER6;
    case  7: return &IER7;
    case  8: return &IER8;
    case  9: return &IER9;
    case 10: return &IER10;
    case 11: return &IER11;
    case 12: return &IER12;
    case 13: return &IER13;
    case 14: return &IER14;
    case 15: return &IER15;
    case 16: return &IER16;
    case 17: return &IER17;
    case 18: return &IER18;
#if CONFIG_EC_ITE_IT5570E
    case 19: return &IER19;
#endif
    default: return 0;
    }
}

// Return ISR register pointer for an IRQ number (ISR_reg = irq / 8).
volatile uint8_t __xdata *gpio_isr(uint8_t irq) {
    switch (irq >> 3) {
    case  0: return &ISR0;
    case  1: return &ISR1;
    case  2: return &ISR2;
    case  3: return &ISR3;
    case  4: return &ISR4;
    case  5: return &ISR5;
    case  6: return &ISR6;
    case  7: return &ISR7;
    case  8: return &ISR8;
    case  9: return &ISR9;
    case 10: return &ISR10;
    case 11: return &ISR11;
    case 12: return &ISR12;
    case 13: return &ISR13;
    case 14: return &ISR14;
    case 15: return &ISR15;
    case 16: return &ISR16;
    case 17: return &ISR17;
    case 18: return &ISR18;
#if CONFIG_EC_ITE_IT5570E
    case 19: return &ISR19;
#endif
    default: return 0;
    }
}

void gpio_irq_enable(const struct Gpio *gpio) {
    volatile uint8_t __xdata *wuemr;
    volatile uint8_t __xdata *wuesr;
    volatile uint8_t __xdata *ier;
    uint8_t mask;

    if (!gpio->wuc_group) return;

    wuemr = gpio_wuemr(gpio->wuc_group);
    wuesr = gpio_wuesr(gpio->wuc_group);
    ier   = gpio_ier(gpio->irq);
    if (!wuemr || !wuesr || !ier) return;

    mask = BIT(gpio->wuc_bit);
    // Arm for the NEXT transition from the current pin state for any-edge detection.
    // WUEMR bit=0 = falling edge, bit=1 = rising edge.
    if (*(gpio->data) & gpio->value)
        *wuemr |= mask;      // pin HIGH: arm for falling edge (bit=1=falling, next transition)
    else
        *wuemr &= ~mask;     // pin LOW: arm for rising edge (bit=0=rising, next transition)
    *wuesr  = mask;          // clear sense register
    *ier   |= BIT(gpio->irq & 7U);
}

void gpio_irq_ack(const struct Gpio *gpio) {
    volatile uint8_t __xdata *wuemr;
    volatile uint8_t __xdata *wuesr;
    volatile uint8_t __xdata *isr_r;
    uint8_t wuc_mask;

    if (!gpio->wuc_group) return;

    wuemr  = gpio_wuemr(gpio->wuc_group);
    wuesr  = gpio_wuesr(gpio->wuc_group);
    isr_r  = gpio_isr(gpio->irq);
    if (!wuemr || !wuesr || !isr_r) return;

    wuc_mask  = BIT(gpio->wuc_bit);
    // Double-clear prevents spurious re-trigger when toggling edge direction
    // while the GPIO is already at the new edge's starting level (WUC_ACK).
    *wuesr  = wuc_mask;
    *wuemr ^= wuc_mask;      // toggle edge direction for any-edge detection
    *wuesr  = wuc_mask;
    *isr_r  = BIT(gpio->irq & 7U);  // clear INTC ISR latch
}

#ifdef GPIO_DEBUG
static void gpio_debug_bank(
    char *bank,
    uint8_t data,
    uint8_t mirror,
    uint8_t pot,
    volatile uint8_t *control
) {
    for (char i = 0; i < 8; i++) {
        DEBUG(
            "%s%d: data %d mirror %d pot %d control %02X\n",
            bank,
            i,
            (data >> i) & 1,
            (mirror >> i) & 1,
            (pot >> i) & 1,
            *(control + i)
        );
    }
}

void gpio_debug(void) {
#define bank(BANK) gpio_debug_bank(#BANK, GPDR##BANK, GPDMR##BANK, GPOT##BANK, &GPCR##BANK##0)
    bank(A);
    bank(B);
    bank(C);
    bank(D);
    bank(E);
    bank(F);
    bank(G);
    bank(H);
    bank(I);
    bank(J);
#define GPOTM 0
    bank(M);
#undef GPOTM
#undef bank
}
#endif // GPIO_DEBUG
