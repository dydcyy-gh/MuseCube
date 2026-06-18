#include "stm32f4xx.h"
#include "usbd_core.h"

#ifndef __USBD_UAC2_CONF_H__
#define __USBD_UAC2_CONF_H__

void audio_v2_init(uint8_t busid, uintptr_t reg_base);
void uac2_play_song_task(void);
void usbd_uac2_deinit(void);

#endif
