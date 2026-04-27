#ifndef __VIDEO_H__
#define __VIDEO_H__

#include "stm32f4xx.h"

uint8_t video_play_init(const char *file);
uint8_t video_play_task(void);
void video_play_deinit(void);

#endif
