/*
 * Copyright (c) 2022 ~ 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* === MuseCube modification ===
 * Updated copyright year range from "2022" to "2022 ~ 2026"
 * Added function declaration: usbd_dfu_get_state() to query DFU device state
 * Added function declarations for DFU download/upload lifecycle: usbd_dfu_begin_load(), usbd_dfu_end_load(), usbd_dfu_reset()
 * Added function declarations for DFU data transfer: usbd_dfu_write(), usbd_dfu_read() with explicit parameter types
 * Removed original user-implemented callbacks: dfu_read_flash(), dfu_write_flash(), dfu_erase_flash(), dfu_leave()
 * The project version centralizes flash I/O internally rather than exposing it to user callback functions
 * === End MuseCube modification === */
#ifndef USBD_DFU_H
#define USBD_DFU_H

#include "usb_dfu.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Init dfu interface driver */
struct usbd_interface *usbd_dfu_init_intf(struct usbd_interface *intf);
uint8_t usbd_dfu_get_state(void);

void usbd_dfu_begin_load(void);
void usbd_dfu_end_load(void);
void usbd_dfu_reset(void);
int usbd_dfu_write(uint16_t value, const uint8_t *data, uint16_t length);
int usbd_dfu_read(uint16_t value, const uint8_t *data, uint16_t length, uint16_t *actual_length);

#ifdef __cplusplus
}
#endif

#endif /* USBD_DFU_H */
