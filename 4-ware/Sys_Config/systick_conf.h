#include "stm32f4xx.h"                  // Device header

#ifndef __SYSTICK_CONF_H__
#define __SYSTICK_CONF_H__

void Systick_init(void);
void Delay_us(uint16_t us);
void Delay_ms(uint16_t ms);

#endif
