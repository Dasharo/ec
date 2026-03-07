// SPDX-License-Identifier: GPL-3.0-only

#ifndef _BOARD_DEBUG_MAILBOX_H
#define _BOARD_DEBUG_MAILBOX_H

// Poll the debug command mailbox at 0x0DF0 and execute any pending command.
// Called from the main loop each iteration.
void mailbox_event(void);

#endif // _BOARD_DEBUG_MAILBOX_H
