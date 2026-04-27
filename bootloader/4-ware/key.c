#include "stm32f4xx.h"

void Wakeup_Key_Init(void) 
{
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE); // 使能GPIOA时钟

    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN;        // 输入模式
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_DOWN;      // 下拉
    GPIO_Init(GPIOA, &GPIO_InitStruct);
}

//按下1 不按0
uint8_t Read_Wakeup_Key(void)
{
	return GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0);
}

