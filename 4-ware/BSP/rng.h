#include "stm32f4xx.h"                  // Device header

#ifndef __RNG_H__
#define __RNG_H__

void RNG_Init(void);
uint32_t RNG_GetRandomRange(uint32_t min, uint32_t max);

/* 新增：基于日期和范围的固定随机数获取函数 */
uint32_t RNG_GetFixedRandomByDate(uint32_t min, uint32_t max);

#endif
