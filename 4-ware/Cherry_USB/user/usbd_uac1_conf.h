#include "stm32f4xx.h"
#include "usbd_core.h"

#ifndef __USBD_UAC1_CONF_H__
#define __USBD_UAC1_CONF_H__

void audio_v1_init(uint8_t busid, uintptr_t reg_base);
void uac1_play_song_task(void);
void usbd_uac1_deinit(void);

#endif
