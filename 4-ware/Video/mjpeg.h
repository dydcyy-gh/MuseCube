#ifndef __MJPEG_H__
#define __MJPEG_H__

#include "stm32f4xx.h"

uint8_t video_mjpeg_play_init(const char *file);
uint8_t video_mjpeg_play_task(void);
void video_mjpeg_play_deinit(void);

#endif
