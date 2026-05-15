#include "stm32f4xx.h"                  // Device header

#ifndef __RTOS_TIMER_H__
#define __RTOS_TIMER_H__		

void ConfigureTimeForRunTimeStats(void);
uint32_t GetRunTimeCounterValue(void);

#endif
