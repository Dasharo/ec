/*
 * Copyright (C) 2023 3mdeb
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdint.h>
#include <stddef.h>
#include <arch/delay.h>
#include <board/gpio.h>
#include <board/espi_mafs.h>
#include <common/debug.h>
#include <ec/espi.h>
#include <ec/scratch.h>
#include "include/espi_mafs/entry.h"

// Include flash ROM
uint8_t __code __at(ESPI_MAFS_OFFSET) espi_mafs_rom[] = {
#include <espi_mafs.h>
};

void espi_enable_mafs(void)
{
    // Disable interrupts
    EA = 0;

    // Use DMA mapping to copy espi_mafs_rom to scratch ROM
    SCARH = 0x80;
    SCARL = (uint8_t)(ESPI_MAFS_OFFSET);
    SCARM = (uint8_t)(ESPI_MAFS_OFFSET >> 8);
    SCARH = 0;

    DEBUG("Switching to ESPI MAFS\n");
    // Jump to flash ROM
    espi_switch_to_mafs();

    // Disable scratch ROM
    SCARH = 0x07;

    DEBUG("Switched to ESPI MAFS\n");
    EA = 1;
}

