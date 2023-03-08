// SPDX-License-Identifier: GPL-3.0-only
//
// TI TPS65993 Type-C Controller

#include <board/gpio.h>
#include <board/smbus.h>
#include <common/macro.h>
#include <common/debug.h>
#include <common/i2c.h>
#include <ec/i2c.h>
#include <ec/smbus.h>

#ifndef TCPC_ADDRESS
    #define TCPC_ADDRESS 0x20
#endif

#define TPS_MAX_LEN 64

// TI defined PD controller registers

// IntEvent/Mask/Clear registers
#define REG_INT_LEN    0xB

#define REG_INT_EVENT1 0x14
#define REG_INT_EVENT2 0x15
#define REG_INT_MASK1  0x16
#define REG_INT_MASK2  0x17
#define REG_INT_CLEAR1 0x18
#define REG_INT_CLEAR2 0x19

// Active Contract PDO register
#define REG_ACT_CONT_PDO_LEN 0x6
#define REG_ACT_CONT_PDO     0x34

// Power Delivery Object
struct pdo {
    uint16_t max_current;
    uint16_t voltage;
    // incomplete
};

#define GPIO_GET_DEBUG(G) { \
    DEBUG("- %s = %s\n", #G, gpio_get(&G) ? "true" : "false"); \
}

// First byte is length
int16_t typec_read(uint8_t command, uint16_t * data, uint8_t length) {
    return i2c_get(&I2C_TYPEC, TCPC_ADDRESS, command, (uint8_t *) data, length + 1);
}

int16_t typec_getlen(uint8_t command, uint8_t * length) {
    return i2c_get(&I2C_TYPEC, TCPC_ADDRESS, command, length, 1);
}

// Special write protocol for TI
int16_t typec_write(uint8_t command, uint8_t * data, uint8_t length) {
    int16_t res = 0;

    res = i2c_start(&I2C_TYPEC, TCPC_ADDRESS, false);
    if (res < 0) return res;

    res = i2c_write(&I2C_TYPEC, &command, 1);
    if (res < 0) return res;

    res = i2c_write(&I2C_TYPEC, &length, 1);
    if (res < 0) return res;

    res = i2c_write(&I2C_TYPEC, data, length);
    if (res < 0) return res;

    i2c_stop(&I2C_TYPEC);

    return res;
}

void typec_init(void) {
	// Set up for i2c usage
	i2c_reset(&I2C_TYPEC, true);

    // Force SMBUS B design to 400kHZ
    SCLKTSB = 0b11;
}

void typec_event(void) {
    static bool last = true;
    uint8_t reg_int[REG_INT_LEN  + 1];
    uint8_t reg_active_pdo[REG_INT_LEN  + 1];
    bool irq;

    irq = gpio_get(&TYPEC_IRQ_N); // active low
    if (last != irq && !irq) {
        // dump IntEvent1
        typec_read(REG_INT_EVENT1, reg_int, REG_INT_LEN);
        DEBUG("New Type-C event: \n");
        DEBUG(" IntEvent1: ");
        for (int k = 1; k <= REG_INT_LEN; k++)
            DEBUG(" %02x", reg_int[k]);
        DEBUG("\n");

        if (reg_int[1] & BIT(3))
            DEBUG("  PlugInsertOrRemoval\n");
        if (reg_int[1] & BIT(4))
            DEBUG("  PRSwapComplete\n");
        if (reg_int[2] & BIT(3))
            DEBUG("  VDMReceived\n"); // Vendor Defined Message. Check VDM RX reg for details. Happens when connecting a Galaxy S21 phone
        if (reg_int[2] & BIT(4)) {
            DEBUG("  NewContractAsCons\n"); // Powered from a source, need to update charger params and PL4
            typec_read(REG_ACT_CONT_PDO, reg_active_pdo, REG_ACT_CONT_PDO_LEN);
            struct pdo active_pdo = { reg_active_pdo[1]      | (reg_active_pdo[2] & 0x3) << 8, // bits 0:9
                                      reg_active_pdo[2] >> 2 | (reg_active_pdo[3] & 0xf) << 6  // bits 10:19
                                    };                                                         // TODO macro?
            DEBUG("   Voltage: %d mV, max current: %d mA\n", active_pdo.voltage * 50, active_pdo.max_current * 10); // Powered from a source, need to update charger params and PL4
        }
        if (reg_int[2] & BIT(5))
            DEBUG("  NewContractAsProv\n"); // Powering a consumer, may need to decrease PL4
        if (reg_int[3] & BIT(4))
            DEBUG("  UsbHostPresent\n");
        if (reg_int[3] & BIT(5))
            DEBUG("  UsbHostPresentNoLonger\n");
        if (reg_int[4] & BIT(0))
            DEBUG("  PowerStatusUpdate\n"); // need to check 0x3F now
        if (reg_int[4] & BIT(1))
            DEBUG("  DataStatusUpdate\n"); // need to check 0x5F now
        if (reg_int[4] & BIT(2))
            DEBUG("  StatusUpdate\n"); // need to check 0x1A now

        // clear interrupt
        typec_read(REG_INT_MASK1, reg_int, REG_INT_LEN);
        typec_write(REG_INT_CLEAR1, reg_int + 1, REG_INT_LEN);
    }

    last = irq;
}

void typec_regdump(void) {
    uint8_t data[TPS_MAX_LEN];
    int16_t res = 0;
    uint8_t len;

    DEBUG("Type-C:\n");
    for (int i = 0; i < 0x08; ++i) {
        for (int j = 0; j < 0x10; ++j) {
            typec_getlen((i << 4) | j, &len);

            DEBUG(" Register %02x size = %x\n", (i << 4) | j, len);
            if (len == 0)
                continue;

            typec_read((i << 4) | j, data, len);
            for (int k = 1; k <= len; k++)
                DEBUG(" %02x", data[k]);
            DEBUG("\n");
        }
    }
}
