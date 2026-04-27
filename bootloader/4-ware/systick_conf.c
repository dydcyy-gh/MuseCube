#include "stm32f4xx.h"           
#include "FreeRTOS.h"
#include "task.h"

//delay and freertos tick

#define MCU_CLK_SPD_MHZ 168

volatile uint32_t RTOS_OK = 0;

extern void xPortSysTickHandler(void);

//systick中断服务函数
void SysTick_Handler(void)
{
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED)
    {
        RTOS_OK = 0;
	}
	else 
	{
		xPortSysTickHandler();
		RTOS_OK = 1;
	}
}

void Systick_init(void)
{
    uint32_t reload;
    SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK);	/* SYSTICK使用外部时钟源,频率为HCLK */
    reload = MCU_CLK_SPD_MHZ*1000000/configTICK_RATE_HZ;/* 根据delay_ostickspersec设定溢出时间,reload为24位寄存器*/
    SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;          /* 开启SYSTICK中断 */
    SysTick->LOAD = reload;                             /* 每1/delay_ostickspersec秒中断一次 */
    SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;           /* 开启SYSTICK */
}

void Delay_us(uint16_t us)
{
    uint32_t ticks;
    uint32_t told, tnow, tcnt = 0;
    uint32_t reload = SysTick->LOAD;
    ticks = us * MCU_CLK_SPD_MHZ;
    told = SysTick->VAL;
    while (1)
    {
        tnow = SysTick->VAL;
        if (tnow != told)
        {
            if (tnow < told) tcnt += told - tnow;
            else tcnt += reload - tnow + told;
            told = tnow;
            if (tcnt >= ticks) break;
        }
    }
}

void Delay_ms(uint16_t ms)
{
    if (__get_IPSR() == 0 && RTOS_OK)
	{
		vTaskDelay(ms);
	}
    else
    {
        uint32_t i;
        for (i=0; i<ms; i++)
        {
            Delay_us(1000);
        }
    }
}

