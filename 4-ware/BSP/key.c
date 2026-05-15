//key.c 摇杆中键和wakeup按键初始化，读取

#include "stm32f4xx.h"
#include "pin_ctrl.h"
#include "variables.h"

void Wakeup_Key_Init(void) 
{
	// 检查是否从Standby模式唤醒
    if (PWR_GetFlagStatus(PWR_FLAG_SB) != RESET) PWR_ClearFlag(PWR_FLAG_SB);
	
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE); // 使能GPIOA时钟

    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN;        // 输入模式
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_DOWN;      // 下拉
    GPIO_Init(GPIOA, &GPIO_InitStruct);
	
	//while(GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0)); //保证按键非按下
}

// 进入待机模式
void Enter_Standby_Mode(void)
{
	Speaker_Power_Ctrl(0);
	Headphone_Power_Ctrl(0);
	
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);  // 使能PWR时钟
    PWR_WakeUpPinCmd(ENABLE);// 使能PA0的WKUP功能（上升沿唤醒）
    PWR_ClearFlag(PWR_FLAG_WU);// 清除唤醒标志（避免误触发）
    PWR_EnterSTANDBYMode();// 进入Standby模式
}

//按下1 不按0
void Read_Wakeup_Key(void)
{
	g_key_WKP_RT = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0);
}

void Stick_Middle_Init(void) 
{
    GPIO_InitTypeDef GPIO_InitStructure;
	
    //PB2--KL_M PC2--KR_M 
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
	
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	
    GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_Init(GPIOC, &GPIO_InitStructure);
}

void Get_Stick_Middle(void) 
{
    g_key_L_M_RT = GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_2) ? 0 : 1;
    g_key_R_M_RT = GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_2) ? 0 : 1;
}
