// SPDX-License-Identifier: GPL-3.0-only

#ifndef _COMMON_RNG_H
#define _COMMON_RNG_H

#include <stdint.h>

void rng_seed(uint16_t seed);
uint16_t simple_rng(void);
uint16_t nondeterministic_rng(void);

#endif
