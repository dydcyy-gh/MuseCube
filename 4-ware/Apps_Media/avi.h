#ifndef __AVI_H__
#define __AVI_H__

#include "stm32f4xx.h"

uint8_t video_avi_play_init(const char *file);
uint8_t video_avi_play_task(void);
void video_avi_play_deinit(void);

#endif
