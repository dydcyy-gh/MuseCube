/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USBD_AUDIO_H
#define USBD_AUDIO_H

#include "usb_audio.h"

#ifdef __cplusplus
extern "C" {
#endif

struct audio_entity_info {
    uint8_t bDescriptorSubtype;
    uint8_t bEntityId;
    uint8_t ep;
};

/* Init audio interface driver */
struct usbd_interface *usbd_audio_init_intf
	(uint8_t busid, struct usbd_interface *intf,uint16_t uac_version,struct audio_entity_info *table,uint8_t num);

#ifdef __cplusplus
}
#endif

#endif /* USBD_AUDIO_H */
