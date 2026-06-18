/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* === MuseCube modification ===
 * Removed `USBD_GAMEPAD_MODE_SWITCH`, `USBD_GAMEPAD_MODE_XBOXONE`, and `USBD_GAMEPAD_MODE_PS4` defines, keeping only `USBD_GAMEPAD_MODE_XINPUT` (value 0)
 * Removed `usbd_gamepad_switch_init_intf()` declaration and `usbd_gamepad_switch_send_report()` declaration, keeping only the XInput variants
 * These removals strip Switch, Xbox One, and PS4 gamepad protocol support, leaving the USB gamepad class limited to XInput only, which matches the project's NES emulator use case requiring only a single gamepad mode
 * === End MuseCube modification === */
#ifndef USBD_GAMEPAD_H
#define USBD_GAMEPAD_H

#include "usb_gamepad.h"

#define USBD_GAMEPAD_MODE_XINPUT  0

struct usbd_interface *usbd_gamepad_xinput_init_intf(struct usbd_interface *intf);
int usbd_gamepad_xinput_send_report(uint8_t ep, struct usb_gamepad_report *report);

#endif /* USBD_GAMEPAD_H */
