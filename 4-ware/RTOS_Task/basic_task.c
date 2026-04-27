#include "stm32f4xx.h" 
#include "FreeRTOS.h"
#include "task.h"
#include "pin_ctrl.h"
#include "key.h"
#include "adc.h"
#include "usb_cc.h"
#include "music.h"
#include "es9018k2m.h"
#include "rtc_clock.h"
#include "v2p_bat.h"
#include "main.h"
#include "debug.h"
#include "lcd_pwm.h"

//此task完成以下轮询读取和处理
//充电检测
//耳机插入检测
//TF卡插入检测
//USB插入检测
//按键读取处理
//摇杆读取处理
//es9018k2m gpio状态
//es9018k2m 更新到芯片
//音乐频谱计算

static uint8_t last_music_bitdepth = 0;
static uint8_t last_hdp_value = 0;
static uint8_t last_brightness = 0;

void Basic_Task( void * pvParameters )
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    static uint8_t count100ms = 0;
	
    //IWTG_Init();
	
    while(1) 
    {
		//IWDG_Feed();
		
        if(g_adc_dma_finished) 
        {
            g_adc_dma_finished = 0;
            ADC_Calculate_Voltage();
            VoltageToPercent();
        }
        ADC_StartConversion();

        Is_Battery_Charging();
        Is_Headphone_Connecting();
        Is_TFcard_Connecting();  
        Read_Wakeup_Key();
        Get_Stick_Middle();
        
		static uint8_t last = 0;
		if(!g_key_WKP_RT && last)
		{
			debug_printf("%d",g_usb_status);
		}
		last = g_key_WKP_RT;
		
		if(last_music_bitdepth != music_bitdepth) 
		{
			if(!ES9018_Set_BitDepth(music_bitdepth))
				last_music_bitdepth = music_bitdepth;
		}
		if(last_hdp_value != g_hdp_value) 
		{
			if(!ES9018_Set_Volume(g_hdp_value,g_hdp_value))
				last_hdp_value = g_hdp_value;
		}
		if(last_brightness != g_brightness) 
		{
			LCD_PWM_SetDuty(g_brightness);
			last_brightness = g_brightness;
		}
		
        if(++count100ms > 4)
        {
            count100ms = 0;
            RTC_Clock_Update();
        }
        
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(20));
    }
}
