#include "stm32f4xx.h"                  // Device header

#ifndef __LCD_PWM_H__
#define __LCD_PWM_H__

void LCD_TIM4_PWM_Init(void);
void LCD_PWM_DeInit(void);

//duty: 0 - 255 (real can 0-280)
void LCD_PWM_SetDuty(uint8_t duty);

//freq: 60k 30k 20k 10k 6k 5k 4k 3k 2k 1k 
//800 600 500 400 300 200 100 80 60 50 
//40 30 20 10 8 6 5 4 3 2 1 
void LCD_PWM_SetFreq(uint16_t freq);

#endif
