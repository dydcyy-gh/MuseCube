#include "stm32f4xx.h"                  // Device header

// RNG初始化
void RNG_Init(void)
{
    RCC_AHB2PeriphClockCmd(RCC_AHB2Periph_RNG, ENABLE);
    RNG_Cmd(ENABLE);
}

// 获取指定范围的随机数
uint32_t RNG_GetRandomRange(uint32_t min, uint32_t max)
{
	while(RNG_GetFlagStatus(RNG_FLAG_DRDY) == RESET);
    uint32_t random = RNG_GetRandomNumber();
    return min + (random % (max - min + 1));
}
