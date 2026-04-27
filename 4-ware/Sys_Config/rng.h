#include "stm32f4xx.h"                  // Device header

#ifndef __RNG_H__
#define __RNG_H__

void RNG_Init(void);
uint32_t RNG_GetRandomRange(uint32_t min, uint32_t max);

#endif
