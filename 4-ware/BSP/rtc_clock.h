#include "stm32f4xx.h"                  // Device header
#include "lunar.h"

#ifndef __RTC_CLOCK_H__
#define __RTC_CLOCK_H__

uint8_t RTC_Set_Clock(void);
uint8_t RTC_Clock_Init(void);
uint8_t RTC_Clock_Update(void);

#endif
