/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* === MuseCube modification ===
 * Removed all runtime function declarations for volume, mute, sampling frequency, and open/close control (usbd_audio_open, usbd_audio_close, usbd_audio_set_volume, usbd_audio_get_volume, usbd_audio_set_mute, usbd_audio_get_mute, usbd_audio_set_sampling_freq, usbd_audio_get_sampling_freq, usbd_audio_get_sampling_freq_table) -- these are not used in the project's audio architecture.
 * Reformatted usbd_audio_init_intf declaration to a single-line parameter layout with minimal spacing.
 * === End MuseCube modification === */
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
