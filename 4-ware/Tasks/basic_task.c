#include "stm32f4xx.h" 
#include "FreeRTOS.h"
#include "task.h"
#include "pin_ctrl.h"
#include "key.h"
#include "adc.h"
#include "music.h"
#include "es9018k2m.h"
#include "max98357A.h"
#include "rtc_clock.h"
#include "v2p_bat.h"
#include "main.h"
#include "debug.h"
#include "lcd_pwm.h"
#include "sdio_sdcard.h"

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
		
		//adc读取
        if(g_adc_dma_finished) 
        {
            g_adc_dma_finished = 0;
            ADC_Calculate_Voltage();
            VoltageToPercent();
        }
		ADC_StartConversion();
		
		//引脚读取
        Is_Battery_Charging();
        Is_Headphone_Connecting();
        Is_TFcard_Connecting();  
        Read_Wakeup_Key();
        Get_Stick_Middle();
		
		//tf卡插入卸载
		if(!g_TFcard_inited && g_TFcard_status)
		{
			if(!SD_Init()) fatfs_mount(DEV_SD);
			else SD_Deinit();
		}
		if(g_TFcard_inited && !g_TFcard_status)
		{
			if(!SD_Deinit()) fatfs_unmount(DEV_SD);
		}

		//pwm ctrl
		if(!g_pwm_inited && g_screen_status)
		{
			LCD_TIM4_PWM_Init();
			LCD_PWM_SetFreq(10000);
		}
		if(g_pwm_inited && !g_screen_status)
		{
			LCD_PWM_DeInit();
		}
		
		//max98357 ctrl
		if(!g_max98357_inited && g_max98357_ststus)
		{
			MAX98357_Init();
		}
		if(g_max98357_inited && !g_max98357_ststus)
		{
			MAX98357_Deinit();
		}
		
		//es9018 ctrl
		if(!g_es9018_inited && g_es9018_status)
		{
			if(ES9018_Init()) ES9018_Deinit();
		}
		if(g_es9018_inited && !g_es9018_status)
		{
			ES9018_Deinit();
		}
		
		//es9018
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
		
		ES9018_Update_Register();
		
        if(++count100ms > 4)
        {
            count100ms = 0;
            RTC_Clock_Update();
        }
        
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(20));
    }
}
