/*
 * Copyright (C) 2020 Evan Lojewski
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdint.h>
#include <board/gpio.h>
#include <ec/espi.h>
#include <espi_mafs/entry.h>

#ifndef HAVE_EC_EN
#define HAVE_EC_EN 1
#endif

#ifndef HAVE_LAN_WAKEUP_N
#define HAVE_LAN_WAKEUP_N 1
#endif

#ifndef HAVE_LED_BAT_CHG
#define HAVE_LED_BAT_CHG 1
#endif

#ifndef HAVE_LED_BAT_FULL
#define HAVE_LED_BAT_FULL 1
#endif

#ifndef HAVE_PCH_DPWROK_EC
#define HAVE_PCH_DPWROK_EC 1
#endif

#ifndef HAVE_PCH_PWROK_EC
#define HAVE_PCH_PWROK_EC 1
#endif

#ifndef HAVE_PM_PWROK
#define HAVE_PM_PWROK 1
#endif

#ifndef HAVE_SLP_SUS_N
#define HAVE_SLP_SUS_N 1
#endif

#ifndef HAVE_SUSWARN_N
#define HAVE_SUSWARN_N 1
#endif

#ifndef HAVE_SUS_PWR_ACK
#define HAVE_SUS_PWR_ACK 1
#endif

#ifndef HAVE_VA_EC_EN
#define HAVE_VA_EC_EN 1
#endif

// Only galp6 has this, so disable by default.
#ifndef HAVE_PD_EN
#define HAVE_PD_EN 0
#endif

#ifndef HAVE_XLP_OUT
#define HAVE_XLP_OUT 1
#endif

// VccPrimary stable (95%) to RSMRST# high
#define tPCH03 delay_ms(10)
// VccDSW 3.3 stable to VccPrimary 1.05V
#define tPCH06 delay_us(200)
// De-assertion of RSMRST# to de-assertion of ESPI_RESET#
#define tPCH18 delay_ms(95)
// RSMRST# de-assertion to SUSPWRDNACK valid
#define tPLT01 delay_ms(200)

#define nop_us(X)   ((uint16_t)((uint32_t)(X) * 1000UL / 186UL))

static void delay_ms(uint8_t ms);
static void delay_us(uint16_t us);

volatile uint8_t __xdata __at(0x1080) FLHCTRL5;

/**
 * Main entry point for switching to ESPI MAFS.
 *
 * NOTE: This *must* be the first function in this file to ensure that it is placed
 *          first in the resulting binary. This is required to ensure that address
 *          matches the address (ESPI_MAFS_OFFSET) for espi_switch_to_mafs in wrapper.c.
 * NOTE: __critical to ensure interrupts are disabled. This does mean that interrupt
 *          such as the timer will be block until flash acccess is complete
 */
// clang-format off
void espi_switch_to_mafs(void) __critical
{
    uint8_t value;
    // At this point we should be already switched to PLL frequency 64.5 MHz in espi_init

    // Power sequence to get RSMRST# low-to-high per IT5570 datasheet

#if HAVE_VA_EC_EN
    // Enable VCCPRIM_* planes - must be enabled prior to USB power in order to
    // avoid leakage
    gpio_set(&VA_EC_EN, true);
#endif // HAVE_VA_EC_EN
#if HAVE_PD_EN
    gpio_set(&PD_EN, true);
#endif
    tPCH06;

    // Enable VDD5
    gpio_set(&DD_ON, true);

#if HAVE_SUS_PWR_ACK
    // De-assert SUS_ACK# - TODO is this needed on non-dsx?
    gpio_set(&SUS_PWR_ACK, true);
#endif // HAVE_SUS_PWR_ACK
    tPCH03;

#if HAVE_PCH_DPWROK_EC
    // Assert DSW_PWROK
    gpio_set(&PCH_DPWROK_EC, true);
#endif // HAVE_PCH_DPWROK_EC

    // De-assert RSMRST#
    gpio_set(&EC_RSMRST_N, true);

    // Wait for PCH stability
    tPCH18;

#if HAVE_EC_EN
    // Allow processor to control SUSB# and SUSC#
    gpio_set(&EC_EN, true);
#endif // HAVE_EC_EN

    // Wait for SUSPWRDNACK validity
    tPLT01;

    gpio_set(&LED_PWR, false);
    gpio_set(&LED_ACIN, true);

    // Wait for flash channel enabled
    do {
        value = ESGCTRL0;
    }while (!(value & BIT(3)));

    // Clear only the flash channel status bit.
    // Other bits will be cleared later
    ESGCTRL0 = BIT(3);

    // Enable uC code fetching via ESPI MAFS
    FLHCTRL5 = BIT(3);

    gpio_set(&LED_PWR, true);
    gpio_set(&LED_ACIN, false);
    // Wait for uC to switch to ESPI MAFS
    do {
        value = FLHCTRL5;
    }while (!(value & BIT(3)));

    gpio_set(&LED_PWR, false);
    gpio_set(&LED_ACIN, false);
    // Do not set SLAVE_BOOT_LOAD_DONE virtual wire yet.
    // It will be done later in espi_event
}

static void delay_ms(uint8_t ms) __critical
{
    for (uint8_t i = ms; i != 0; i--) {
        delay_us(1000);
    }
}

// With a PLL of 64MHz one machine cycle will take 12 / 64.5 MHz ~ 186ns
static void delay_us(uint16_t us) __critical
{
    for (uint16_t i = nop_us(us); i != 0; i--) {
        __asm__("nop");
    }
}

