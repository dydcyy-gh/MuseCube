#include "stm32f4xx.h"
#include "variables.h"

//freq : 84MHZ / PSC / ARR  (TIM8时钟为84MHz)
//duty : CCR / ARR
uint16_t PWM_PSC = 60;     // 预分频值
uint16_t PWM_ARR = 1400;   // 自动重装载值

void LCD_TIM4_PWM_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStruct;
    TIM_OCInitTypeDef TIM_OCInitStruct;

    // 1. 使能时钟 (TIM8在APB2上)
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM8, ENABLE);

    // 2. 配置GPIO为复用功能
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOC, &GPIO_InitStruct);

    // 3. 配置引脚复用映射 (TIM8通道1)
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource6, GPIO_AF_TIM8);

	// 4. 配置TIM8基本参数
    TIM_TimeBaseInitStruct.TIM_Period = PWM_ARR - 1;       // ARR
    TIM_TimeBaseInitStruct.TIM_Prescaler = PWM_PSC - 1;    // PSC
    TIM_TimeBaseInitStruct.TIM_ClockDivision = 0;
    TIM_TimeBaseInitStruct.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInitStruct.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM8, &TIM_TimeBaseInitStruct);

    // 5. 配置PWM模式 (通道1)
    TIM_OCStructInit(&TIM_OCInitStruct);
    
    TIM_OCInitStruct.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStruct.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStruct.TIM_OCPolarity = TIM_OCPolarity_Low;

    TIM_OC1Init(TIM8, &TIM_OCInitStruct);

    // 6. 启用主输出 (高级定时器必须)
    TIM_CtrlPWMOutputs(TIM8, ENABLE);

    // 7. 启动定时器
    TIM_Cmd(TIM8, ENABLE);
	
	g_screen_pwm = 1;
}

// 关闭PWM，并配置引脚为推挽输出低电平
void LCD_PWM_DeInit(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;

    // 1. 停止定时器
    TIM_Cmd(TIM8, DISABLE);

    // 2. 禁用PWM主输出
    TIM_CtrlPWMOutputs(TIM8, DISABLE);

    // 3. 将PC6引脚配置为推挽输出，并拉低
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;   // 无上下拉
    GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_ResetBits(GPIOC, GPIO_Pin_6);  // 输出低电平
	
	g_screen_pwm = 0;
}

//duty: 0 - 255 (real can 0-280)
void LCD_PWM_SetDuty(uint8_t duty) { TIM8->CCR1 = 5 * (duty+5); }

//freq: 60k 30k 20k 10k 6k 5k 4k 3k 2k 1k 
//800 600 500 400 300 200 100 80 60 50 
//40 30 20 10 8 6 5 4 3 2 1 
void LCD_PWM_SetFreq(uint16_t freq) { TIM8->PSC = (60000 / freq) - 1; }
