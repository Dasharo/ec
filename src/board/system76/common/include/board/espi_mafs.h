/*
 * Copyright (C) 2020 Evan Lojewski
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef _BOARD_ESPI_MAFS_H
#define _BOARD_ESPI_MAFS_H

#include <stdint.h>


/**
 * Switch code fetching from flash to ESPI MAFS
 */
void espi_enable_mafs(void);

#endif // _BOARD_FLASH_H
