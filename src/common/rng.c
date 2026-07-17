// SPDX-License-Identifier: GPL-3.0-only
//
#include <common/rng.h>
#include <arch/time.h>

#define LFSR_MASK 0xB400u

static uint16_t simple_rng_reg = -1U;
static uint16_t nondeterministic_rng_reg = -1U;

void rng_seed(uint16_t seed) {
    // 0 is invalid state, the generator locks up on 0
    uint16_t s = seed ? seed : -1U;
    simple_rng_reg = s;
    nondeterministic_rng_reg = s;
}

uint16_t _galois_fsr(uint16_t lfsr) {
    uint16_t bit = lfsr & 1U;
    lfsr >>= 1U;
    if (bit) {
        lfsr ^= LFSR_MASK;
    }
    return lfsr;
}

uint16_t simple_rng(void) {
    simple_rng_reg = _galois_fsr(simple_rng_reg);
    return simple_rng_reg;
}

uint16_t nondeterministic_rng(void) {
    nondeterministic_rng_reg += (uint16_t)(time_get());
    nondeterministic_rng_reg = _galois_fsr(nondeterministic_rng_reg);
    return nondeterministic_rng_reg;
}
