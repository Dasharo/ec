// SPDX-License-Identifier: GPL-3.0-only
//
// TI TPS65993 Type-C Controller

#include <board/smbus.h>
#include <common/macro.h>
#include <common/debug.h>
#include <common/i2c.h>
#include <ec/i2c.h>
#include <ec/smbus.h>

#ifndef TYPEC_ADDRESS
    #define TYPEC_ADDRESS 0x20
#endif

// Registers
#define REG_ACTIVE_CONTRACT_PDO 0x34

int16_t typec_read(uint8_t address, uint8_t command, uint64_t * data, uint16_t length) {
    return i2c_get(&I2C_TYPEC, address, command, (uint8_t *)data, length + 1);
}

int16_t typec_write(uint8_t address, uint8_t command, uint16_t data) {
    return i2c_set(&I2C_TYPEC, address, command, (uint8_t *)&data, 2);
}

void typec_init(void) {
	// Set up for i2c usage
	i2c_reset(&I2C_TYPEC, true);

    // Force SMBUS B design to 100kHZ
    SCLKTSB = 0b10;
}

void typec_debug(void) {
    uint64_t data = 0;
    int16_t res = 0;

    #define command(N, A, V, L) { \
        DEBUG("  " #N "\t: "); \
        res = typec_read(A, V, &data, L + 1); \
        if (res < 0) { \
            DEBUG("ERROR %04X\n", -res); \
        } else { \
            DEBUG("%0x\n", data); \
        } \
    }

    DEBUG("Type-C:\n");
    command(VID, TYPEC_ADDRESS, 0x00, 4);
    command(DID, TYPEC_ADDRESS, 0x01, 4);
    command(ProtoVer, TYPEC_ADDRESS, 0x02, 4);
    command(Mode, TYPEC_ADDRESS, 0x03, 4);
    command(Type, TYPEC_ADDRESS, 0x04, 4);
    command(IntEvent1, TYPEC_ADDRESS, 0x14, 8);
    command(IntEvent2, TYPEC_ADDRESS, 0x15, 8);
    command(IntMask1, TYPEC_ADDRESS, 0x16, 8);
    command(IntMask2, TYPEC_ADDRESS, 0x17, 8);
    command(Status, TYPEC_ADDRESS, 0x1a, 4);
    command(BootFlags, TYPEC_ADDRESS, 0x2d, 12);
    command(ActiveContractPDO, TYPEC_ADDRESS, 0x34, 6);
    command(ActiveContractRDO, TYPEC_ADDRESS, 0x35, 4);
    command(SinkRequestRDO, TYPEC_ADDRESS, 0x36, 4);
    command(PowerStatus, TYPEC_ADDRESS, 0x3F, 2);
    command(PDStatus, TYPEC_ADDRESS, 0x40, 4);
    command(DataStatus, TYPEC_ADDRESS, 0x5F, 4);

    #undef command
}
