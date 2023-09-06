/*
 * Copyright (C) 2020 Evan Lojewski
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdint.h>
#include <stddef.h>
#include <board/espi_mafs.h>
#include <common/debug.h>
#include <ec/espi.h>
#include <ec/scratch.h>
#include "include/espi_mafs/entry.h"

// Include flash ROM
uint8_t __code __at(ESPI_MAFS_OFFSET) espi_mafs_rom[] = {
#include <espi_mafs.h>
};

void espi_enable_mafs(void) {
    uint8_t value;
    // Use DMA mapping to copy flash ROM to scratch ROM
    SCARH = 0x80;
    SCARL = (uint8_t)(ESPI_MAFS_OFFSET);
    SCARM = (uint8_t)(ESPI_MAFS_OFFSET >> 8);
    SCARH = 0;

    // Jump to flash ROM
    espi_switch_to_mafs();

    // Disable scratch ROM
    SCARH = 0x07;

    // Set auto SUS_ACK# and Boot_Load_Done/Status bits for MAFS
    value = VWCTRL5;
    VWCTRL5 = value | BIT(0) | BIT(1);
}

