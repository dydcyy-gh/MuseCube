/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USBD_GAMEPAD_H
#define USBD_GAMEPAD_H

#include "usb_gamepad.h"

#define USBD_GAMEPAD_MODE_XINPUT  0

struct usbd_interface *usbd_gamepad_xinput_init_intf(struct usbd_interface *intf);
int usbd_gamepad_xinput_send_report(uint8_t ep, struct usb_gamepad_report *report);

#endif /* USBD_GAMEPAD_H */
