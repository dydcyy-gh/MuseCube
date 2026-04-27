#include <stm32f4xx.h>

/*
 * 配置 TIM2 用于 FreeRTOS 运行时间统计
 * TIM2 是 32 位定时器，非常适合做这个
 * 假设 APB1 总线频率为 42MHz (SystemCoreClock/4)
 * TIM2 时钟 = 42MHz * 2 = 84MHz
 */
void ConfigureTimeForRunTimeStats(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    
    // 1. 使能 TIM2 时钟 (APB1)
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    
    // 2. 配置定时器
    // 目标频率：50kHz (每 20us 计数一次)
    // Prescaler = (84MHz / 50kHz) - 1 = 1680 - 1
    TIM_TimeBaseStructure.TIM_Prescaler = 1680 - 1; 
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;  
    TIM_TimeBaseStructure.TIM_Period = 0xFFFFFFFF;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;     
    
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);
    
    // 3. 启动定时器
    TIM_Cmd(TIM2, ENABLE);
}

/* 
 * 获取当前运行时间计数值 
 * 直接返回 TIM2 的 32 位计数值
 */
uint32_t GetRunTimeCounterValue(void)
{
    return TIM_GetCounter(TIM2);
}
