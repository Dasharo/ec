// SPDX-License-Identifier: GPL-3.0-only

// Debug command mailbox at fixed XDATA address 0x0DF0.
//
// The DBGR/SMB hardware slave allows an external host to write to any EC
// XDATA address via the battery SMBus pins. Write a non-zero command byte
// to trigger execution; firmware clears it on completion.
//
// Host protocol:
//   1. Write command byte to 0x0DF0 via DBGR/SMB memory write
//   2. Poll 0x0DF0 until it reads 0x00 (command complete)
//   3. Read status from 0x0DF1: 0=idle, 1=busy, 2=done

#include <stdint.h>

#include <board/debug_mailbox.h>
#include <board/power.h>
#include <common/debug.h>

#define DBG_CMD_POWER_OFF  0x01
#define DBG_CMD_POWER_ON   0x02

#define MAILBOX_STATUS_IDLE  0x00
#define MAILBOX_STATUS_BUSY  0x01
#define MAILBOX_STATUS_DONE  0x02

static volatile uint8_t __xdata __at(0x0DF0) mailbox_cmd;
static volatile uint8_t __xdata __at(0x0DF1) mailbox_status;

void mailbox_event(void) {
    uint8_t cmd = mailbox_cmd;
    if (!cmd)
        return;

    DEBUG("mailbox: cmd=0x%02X\n", cmd);
    mailbox_status = MAILBOX_STATUS_BUSY;

    switch (cmd) {
    case DBG_CMD_POWER_OFF:
        power_off();
        break;
    case DBG_CMD_POWER_ON:
        power_on();
        break;
    default:
        DEBUG("mailbox: unknown cmd 0x%02X\n", cmd);
        break;
    }

    mailbox_status = MAILBOX_STATUS_DONE;
    mailbox_cmd = 0;
}
