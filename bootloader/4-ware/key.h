#include "stm32f4xx.h"                  // Device header

#ifndef __KEY_H__
#define __KEY_H__

void Wakeup_Key_Init(void);
uint8_t Read_Wakeup_Key(void);

#endif
