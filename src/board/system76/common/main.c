// SPDX-License-Identifier: GPL-3.0-only

#include <8051.h>
#include <stdio.h>

#include <arch/arch.h>
#include <arch/delay.h>
#include <arch/time.h>
#include <board/battery.h>
#include <board/board.h>
#include <board/dgpu.h>
#include <board/ecpm.h>
#include <board/espi.h>
#include <board/fan.h>
#include <board/gpio.h>
#include <board/gctrl.h>
#include <board/irq.h>
#include <board/kbc.h>
#include <board/kbled.h>
#include <board/kbscan.h>
#include <board/keymap.h>
#include <board/lid.h>
#include <board/options.h>
#include <board/peci.h>
#include <board/pmc.h>
#include <board/power.h>
#include <board/ps2.h>
#include <board/pwm.h>
#include <board/smbus.h>
#include <board/smfi.h>
#include <board/usbpd.h>
#include <common/debug.h>
#include <common/macro.h>
#include <common/version.h>
#include <ec/ec.h>
#include <ec/intc.h>
#include <ec/kbscan.h>

#ifdef PARALLEL_DEBUG
#include <board/parallel.h>
#endif // PARALLEL_DEBUG

// --------------------------------------------------------------------------
// SINK_CTRL interrupt routing. Boards override SINK_CTRL_IRQ in gpio.h
// if SINK_CTRL is not on GPC3 (e.g. GPH7 on v560tnx/v540tnx → IRQ 148).
// WUC/IER/ISR registers are derived automatically from the GPIO struct.
// --------------------------------------------------------------------------
#ifndef HAVE_SINK_CTRL
#define HAVE_SINK_CTRL 0
#endif

// Default IRQ for GPC3 (nv40mz, galp5, galp6): WU108, Group 11 bit 4, INT113
#ifndef SINK_CTRL_IRQ
#define SINK_CTRL_IRQ   _GPIO_WUC_IRQ_C3   // 113
#endif

// --------------------------------------------------------------------------
// Default interrupt routing for JACK_IN_N (boards can override in gpio.h)
// GPC6 = WUEMR2[3] -> INT6 = IER0[6]
// --------------------------------------------------------------------------
#ifndef HAVE_JACK_IN_N
#define HAVE_JACK_IN_N 0
#endif

// --------------------------------------------------------------------------
// Pending flags — set by external_1 ISR, cleared by main loop handlers.
// kbscan_irq_pending and kbscan_matrix[] are defined in kbscan.c.
// dgpu_irq_pending is defined in dgpu.c.
// --------------------------------------------------------------------------
volatile bool acin_irq_pending       = false;
volatile bool pwr_sw_irq_pending     = false;
volatile bool sys_pwrgd_irq_pending  = false;
volatile bool plt_rst_irq_pending    = false;
volatile bool slp_sus_irq_pending    = false;
volatile bool lan_wakeup_irq_pending = false;
volatile bool usbpd_irq_pending      = false;
volatile bool lid_irq_pending        = false;
volatile bool smfi_irq_pending       = false;
volatile bool kbc_irq_pending        = false;
volatile bool pmc_irq_pending        = false;
volatile bool espi_irq_pending       = false;

void external_0(void) __interrupt(0) {}
// timer_0 is in time.c
void timer_0(void) __interrupt(1);
void timer_1(void) __interrupt(3) {}
void serial(void) __interrupt(4) {}
void timer_2(void) __interrupt(5) {}

static volatile uint8_t last_irq = 0;

// --------------------------------------------------------------------------
// external_1 ISR: dispatches INTC interrupts to pending flags
// --------------------------------------------------------------------------
void external_1(void) __interrupt(2) {
    uint8_t irq = intc_get_irq();
    last_irq = irq;
    switch (irq) {
#if HAVE_JACK_IN_N
    case _GPIO_WUC_IRQ_C6:  // JACK_IN_N (GPC6 → INT6)
        gpio_irq_ack(&JACK_IN_N);
        usbpd_irq_pending = true;
        break;
#endif
    case _GPIO_WUC_IRQ_B3:  // PWR_SW_N (GPB3 → INT14)
        gpio_irq_ack(&PWR_SW_N);
        pwr_sw_irq_pending = true;
        break;
    case _GPIO_WUC_IRQ_D2:  // BUF_PLT_RST_N (GPD2 → INT17)
        gpio_irq_ack(&BUF_PLT_RST_N);
        plt_rst_irq_pending = true;
        break;
    case 22:  // SMFI semaphore (INT22 = IER2[6])
        ISR2 = BIT(6); // write-1-to-clear: unblock IVCT for other pending IRQs
        smfi_irq_pending = true;
        break;
    case 24:  // KBC IBF (INT24 = IER3[0])
        ISR3 = BIT(0); // clear edge-triggered ISR latch so INT24 doesn't lock out lower IRQs
        kbc_irq_pending = true;
        break;
    case 25:  // PMC1 IBF (INT25 = IER3[1])
        ISR3 = BIT(1);
        pmc_irq_pending = true;
        break;
#if HAVE_PD_IRQ
    case _GPIO_WUC_IRQ_E2:  // PD_IRQ (GPE2 → INT74)
        gpio_irq_ack(&PD_IRQ);
        usbpd_irq_pending = true;
        break;
#endif
    case 84:  // KSM scan data valid (INT84 = IER10[4])
        {
            uint8_t c;
            for (c = 0; c < KM_OUT; c++)
                kbscan_matrix[c] = ~KSO_LSDR[c]; // invert: 1=pressed
        }
        SDSR = BIT(0);   // clear SDV (write-1-to-clear)
        ISR10 = BIT(4);  // clear ISR latch
        kbscan_irq_pending = true;
        break;
#if HAVE_DGPU
    case _GPIO_WUC_IRQ_H4:  // DGPU_PWR_EN (GPH4 → INT85, WU88 Group 9 bit 0)
        gpio_irq_ack(&DGPU_PWR_EN);
        dgpu_irq_pending = true;
        break;
#endif
#if HAVE_LAN_WAKEUP_N
    case _GPIO_WUC_IRQ_B2:  // LAN_WAKEUP_N (GPB2 → INT92)
        gpio_irq_ack(&LAN_WAKEUP_N);
        lan_wakeup_irq_pending = true;
        break;
#endif
    case _GPIO_WUC_IRQ_C0:  // ALL_SYS_PWRGD (GPC0 → INT93)
        gpio_irq_ack(&ALL_SYS_PWRGD);
        sys_pwrgd_irq_pending = true;
        break;
    case _GPIO_WUC_IRQ_B0:  // ACIN_N (GPB0 → INT106, WU101 Group 10 bit 5)
        gpio_irq_ack(&ACIN_N);
        acin_irq_pending = true;
        break;
    case _GPIO_WUC_IRQ_B1:  // LID_SW_N (GPB1 → INT107, WU102 Group 10 bit 6)
        gpio_irq_ack(&LID_SW_N);
        lid_irq_pending = true;
        break;
#if HAVE_SINK_CTRL
    case SINK_CTRL_IRQ:  // SINK_CTRL (board-specific pin → IRQ from gpio_wuc.h)
        gpio_irq_ack(&SINK_CTRL);
        usbpd_irq_pending = true;
        break;
#endif
#if HAVE_DGPU
    case _GPIO_WUC_IRQ_J3:  // GC6_FB_EN (GPJ3 → INT131)
        gpio_irq_ack(&GC6_FB_EN);
        dgpu_irq_pending = true;
        break;
#endif
#if HAVE_SLP_SUS_N
    case _GPIO_WUC_IRQ_J7:  // SLP_SUS_N (GPJ7 → INT135)
        gpio_irq_ack(&SLP_SUS_N);
        slp_sus_irq_pending = true;
        break;
#endif
#if CONFIG_BUS_ESPI
    case 154: // eSPI VW (INT154 = IER19[2])
        ISR19 = BIT(2); // clear ISR latch
        espi_irq_pending = true;
        break;
#endif
    case 159: // PLL Frequency Change Event (edge-triggered, always-enabled)
        ISR19 = BIT(7); // write-1-to-clear: unblock IVCT for other pending IRQs
        break;
    }
}

uint8_t main_cycle = 0;

// --------------------------------------------------------------------------
// intc_init: configure WUC edge detection and enable IER bits
// --------------------------------------------------------------------------
static void intc_init(void) {
    // Clear all IER registers for a clean slate (prevents stale bits from
    // power-up or previous firmware, including IER19[7] = IRQ159 if maskable)
    IER0 = 0; IER1 = 0; IER2 = 0; IER3 = 0;
    IER4 = 0; IER5 = 0; IER6 = 0; IER7 = 0;
    IER8 = 0; IER9 = 0; IER10 = 0; IER11 = 0;
    IER12 = 0; IER13 = 0; IER14 = 0; IER15 = 0;
    IER16 = 0; IER17 = 0; IER18 = 0;
#if CONFIG_EC_ITE_IT5570E
    IER19 = 0; IER20 = 0; IER21 = 0;
#endif

    // GPIO edge-detect interrupts: WUC/IER registers derived from struct Gpio fields.
    // gpio_irq_enable() sets rising edge initially, clears WUESR, and enables IER bit.

#if HAVE_JACK_IN_N
    gpio_irq_enable(&JACK_IN_N);
#endif

    gpio_irq_enable(&PWR_SW_N);
    gpio_irq_enable(&BUF_PLT_RST_N);

#if HAVE_PD_IRQ
    gpio_irq_enable(&PD_IRQ);
#endif

    // INT84: KSM scan data valid. Clear any stale SDV before enabling IER so
    // the INTC sees a clean rising edge when the first scan completes.
    IER10 |= BIT(4);
    SDSR = BIT(0);

#if HAVE_DGPU
    gpio_irq_enable(&DGPU_PWR_EN);
    gpio_irq_enable(&GC6_FB_EN);
#endif

#if HAVE_LAN_WAKEUP_N
    gpio_irq_enable(&LAN_WAKEUP_N);
#endif

    gpio_irq_enable(&ALL_SYS_PWRGD);
    gpio_irq_enable(&ACIN_N);

    // LID_SW_N: gpio_irq_enable arms for the correct edge based on current state.
    gpio_irq_enable(&LID_SW_N);

#if HAVE_SINK_CTRL
    gpio_irq_enable(&SINK_CTRL);
#endif

#if HAVE_SLP_SUS_N
    gpio_irq_enable(&SLP_SUS_N);
#endif

    // Non-GPIO interrupts (no WUC): enable IER bits directly.
    IER2 |= BIT(6);         // SMFI semaphore (INT22)
    IER3 |= BIT(0) | BIT(1); // KBC IBF (INT24), PMC1 IBF (INT25)

    // eSPI VW (INT154 = IER19[2])
#if CONFIG_BUS_ESPI
    IER19 |= BIT(2);
#endif

    // Enable 8051 external interrupt 1 (EX1 bit in IE register)
    IE |= 0x04;
}

void init(void) {
    // Must happen first
    arch_init();
    ec_init();
    gctrl_init();
    gpio_init();

    // Can happen in any order
#if HAVE_DGPU
    dgpu_init();
#endif
    ecpm_init();
    kbc_init();
    kbled_init();
#ifdef PARALLEL_DEBUG
    parallel_debug = false;
    if (parallel_init()) {
        parallel_debug = true;
    } else
#endif // PARALLEL_DEBUG
    {
        kbscan_init();
    }
    keymap_init();
    options_init();
    peci_init();
    pmc_init();
    pwm_init();
    smbus_init();
    smfi_init();
    usbpd_init();
    ps2_init();

    intc_init();

    // Must happen last
    power_init();
    board_init();
}

void main(void) {
    init();

    INFO("\n");

#ifdef GPIO_DEBUG
    gpio_debug();
#endif

    INFO("System76 EC board '%s', version '%s'\n", board(), version());
    ec_print_reset_reason();

    // Set the initial lid and AC states
    lid_event();
    acin_event();

    // Sequence the board to the initial state
    usbpd_event();
    // If we were woken by power button, power on.
    if (!gpio_get(&PWR_SW_N) && gpio_get(&LID_SW_N))
        power_on();
    else
        power_off();
    update_power_state();

    for (main_cycle = 0;; main_cycle++) {
        // Idle until next interrupt (~1ms timer_0 or INTC wakeup)
        PCON |= 1;
        if (last_irq) {
            DEBUG("IRQ %u\n", last_irq);
            last_irq = 0;
        }

        // USB-PD signals: JACK_IN_N, SINK_CTRL, PD_IRQ
        if (usbpd_irq_pending) {
            usbpd_irq_pending = false;
            usbpd_event();
        }

        // AC adapter plug/unplug: also refreshes USB-PD current limits
        if (acin_irq_pending) {
            acin_irq_pending = false;
            usbpd_event();
            acin_event();
        }

        // Power button press/release
        if (pwr_sw_irq_pending) {
            pwr_sw_irq_pending = false;
            pwr_sw_event();
        }

        // Platform reset de-assertion (BUF_PLT_RST_N)
        if (plt_rst_irq_pending) {
            plt_rst_irq_pending = false;
            plt_rst_event();
        }

        // System power good (ALL_SYS_PWRGD)
        if (sys_pwrgd_irq_pending) {
            sys_pwrgd_irq_pending = false;
            update_power_state();
            sys_pwrgd_event();
        }

        // Suspend (SLP_SUS_N) — debug logging only
#if HAVE_SLP_SUS_N
        if (slp_sus_irq_pending) {
            slp_sus_irq_pending = false;
            slp_sus_event();
        }
#endif

        // LAN remote wakeup
#if HAVE_LAN_WAKEUP_N
        if (lan_wakeup_irq_pending) {
            lan_wakeup_irq_pending = false;
            lan_wakeup_event();
        }
#endif

        if (lid_irq_pending) {
            lid_irq_pending = false;
            lid_event();
        }

        // No interrupt possible until SMFI Semaphore register is used
        smfi_event();

        // INT24 (IBF set): host wrote a command/data byte — process it.
        if (kbc_irq_pending) {
            kbc_irq_pending = false;
            kbc_event(&KBC);
        }
        // kbc_event() never processes IBF and writes OBF in the same call
        // (kbc_clear_output() needs a loop iteration to settle). Drive the
        // OBF side by polling kbc_output_pending() — no interrupt available.
        if (kbc_output_pending()) {
            kbc_event(&KBC);
        }

        if (pmc_irq_pending) {
            pmc_irq_pending = false;
            pmc_event(&PMC_1);
        }

#if CONFIG_BUS_ESPI
        if (espi_irq_pending) {
            espi_irq_pending = false;
            espi_event();
        }
#endif

        // Keyboard: new scan data (INT84) or key held with repeat pending.
        // INT84 fires only on scan change, so kbscan_repeat_active keeps
        // kbscan_event() running via timer_0 (~1ms) while a key is held.
#ifdef PARALLEL_DEBUG
        if (!parallel_debug)
#endif // PARALLEL_DEBUG
        if (kbscan_irq_pending || kbscan_repeat_active) {
            kbscan_irq_pending = false;
            kbscan_event();
        }

#if HAVE_DGPU
        if (dgpu_irq_pending) {
            dgpu_irq_pending = false;
            // Re-evaluate fan curves when dGPU state changes
            fan_duty_set(peci_get_fan_duty(), dgpu_get_fan_duty());
        }
#endif

        // Board-specific events
        board_event();

        // Periodic tasks driven by 50 ms timer_0 flag.
        // Counters tick at 50 ms each: power=2 (100ms), fan=5 or 20 (250ms or 1000ms),
        // battery=20 (1000ms). No time_get() needed.
        if (timer_50ms_pending) {
            static uint8_t power_ticks = 0;
            static uint8_t fan_ticks = 0;
            static uint8_t batt_ticks = 0;
            timer_50ms_pending = false;

            // Power/usbpd: debounce timeouts, usbpd_check_mode — every 100 ms
            if (++power_ticks >= 2) {
                power_ticks = 0;
                usbpd_event();
                sus_pwrdn_event();
                power_led_event();
            }

            // Fan: smooth=250 ms (5 ticks), normal=1000 ms (20 ticks)
            if (++fan_ticks >= (SMOOTH_FANS != 0 ? 5 : 20)) {
                fan_ticks = 0;
                fan_duty_set(peci_get_fan_duty(), dgpu_get_fan_duty());
            }

            // Battery: every 1000 ms (20 ticks)
            if (++batt_ticks >= 20) {
                batt_ticks = 0;
                battery_event();
            }
        }
    }
}
