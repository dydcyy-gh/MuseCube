#include "stm32f4xx.h"

#ifndef __GIF_H__
#define __GIF_H__

uint8_t Decode_GIF_Init(const char *path);
uint8_t Decode_GIF_Task(void);
void    Decode_GIF_Deinit(void);

#endif
