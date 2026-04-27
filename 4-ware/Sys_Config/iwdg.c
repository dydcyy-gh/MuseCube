#include "stm32f4xx.h"                  // Device header

// IWTG初始化
void IWTG_Init(void)
{
    // 1. 使能对 IWDG 寄存器的写访问
    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);

    // 2. 设置预分频器为 64，计数器时钟 = LSI(30-60KHz) / 128 = 234 - 468 Hz
    IWDG_SetPrescaler(IWDG_Prescaler_128);

    // 3. 设置重装载值, 超时时间 ≈ (938 / 234-468) = 2 - 4 秒
    IWDG_SetReload(938);

    // 4. 重装载计数器（初次喂狗）
    IWDG_ReloadCounter();

    // 5. 使能 IWDG
    IWDG_Enable();
}

// 喂狗函数：在主循环中定期调用
void IWDG_Feed(void)
{
    IWDG_ReloadCounter();
}
